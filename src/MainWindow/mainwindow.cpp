#include "mainwindow.h"
#include "src/MainWindow/ui_mainwindow.h"

#include "AddAccountDialog.h"
#include "../Managers/ConnectionManager.h"
#include "../Managers/EmailManager.h"
#include "../Managers/UserManager.h"

#include <QFile>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QTableWidgetItem>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , userManager(new UserManager(this))
    , connectionManager(new ConnectionManager(this))
    , emailManager(new EmailManager(connectionManager, this))
{
    ui->setupUi(this);

    setWindowTitle("Email Manager");

    setupThemeComboBox();
    setupConnections();
    loadUserAccounts();

    // Setup tables
    ui->accountsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->accountsTable->setHorizontalHeaderLabels({"Username", "E-Mail"});
    ui->senderTableWidget->setColumnCount(2);
    ui->senderTableWidget->setHorizontalHeaderLabels({"Sender (Email)", "Count"});
    ui->senderTableWidget->horizontalHeader()->setStretchLastSection(false);
    ui->senderTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->senderTableWidget->setColumnWidth(1, 80);

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->progressBar->setVisible(false);

    setupUserAndRepositorySelection();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupThemeComboBox()
{
    ui->themeComboBox->addItems({"Light", "Dark"});
    QSettings settings("YourCompany", "YourApp");
    QString savedTheme = settings.value("theme", "Light").toString();
    applyTheme(savedTheme);
    ui->themeComboBox->setCurrentText(savedTheme);

    connect(ui->themeComboBox, &QComboBox::currentTextChanged,
            this, &MainWindow::applyTheme);
}

void MainWindow::applyTheme(const QString &theme)
{
    QDir baseDir(QCoreApplication::applicationDirPath());
    baseDir.cdUp();
    baseDir.cdUp();
    baseDir.cd("src/themes");

    QString filePath = baseDir.filePath(theme == "Dark" ? "dark.qss" : "light.qss");
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to load theme:" << filePath;
        return;
    }
    qApp->setStyleSheet(file.readAll());
    file.close();

    QSettings settings("VaskoCorp", "email-manager");
    settings.setValue("theme", theme);
}

void MainWindow::setupConnections()
{
    // ConnectionManager signals
    connect(connectionManager, &ConnectionManager::connectionStatus, this, &MainWindow::onConnectionStatus);
    connect(connectionManager, &ConnectionManager::repositoriesFetched, this, &MainWindow::onRepositoriesFetched);

    // EmailManager signals
    connect(emailManager, &EmailManager::cleanupCompleted, this, &MainWindow::onCleanupCompleted);
    connect(emailManager, &EmailManager::progressUpdated, this, &MainWindow::updateProgress);

    // populate senderTableWidget
    connect(emailManager, &EmailManager::sendersFetched, this, [this](const QMap<QString, int> &senders) {
        ui->senderTableWidget->clearContents();
        ui->senderTableWidget->setRowCount(0);

        int row = 0;
        for (auto it = senders.begin(); it != senders.end(); ++it) {
            ui->senderTableWidget->insertRow(row);
            QTableWidgetItem *senderItem = new QTableWidgetItem(it.key());
            senderItem->setCheckState(Qt::Unchecked);
            QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(it.value()));
            ui->senderTableWidget->setItem(row, 0, senderItem);
            ui->senderTableWidget->setItem(row, 1, countItem);
            row++;
        }
        qDebug() << "Senders populated in senderTableWidget.";
    });

    // Also connect for progress if needed
    //connect(connectionManager, &ConnectionManager::progressUpdated, this, &MainWindow::updateProgress);

    // "Add Account"
    connect(ui->addAccountButton, &QPushButton::clicked, this, [this]() {
        AddAccountDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            QString server   = dialog.getServer();
            QString username = dialog.getUsername();
            QString password = dialog.getPassword();

            if (userManager->addUser(server, username, password)) {
                addAccountToTable(username, server);
                if (ui->comboBox->findText(username) == -1) {
                    ui->comboBox->addItem(username);
                }
                qDebug() << "Account added successfully.";
            } else {
                qDebug() << "Account already exists.";
            }
        }
    });

    // "Remove Account"
    connect(ui->removeAccountButton, &QPushButton::clicked, this, [this]() {
        int currentRow = ui->accountsTable->currentRow();
        if (currentRow == -1) {
            qDebug() << "No account selected for removal.";
            return;
        }

        QTableWidgetItem *usernameItem = ui->accountsTable->item(currentRow, 0);
        if (!usernameItem) {
            qDebug() << "Failed to fetch username from selected row.";
            return;
        }
        QString username = usernameItem->text();

        if (userManager->removeUser(username)) {
            ui->accountsTable->removeRow(currentRow);
            int index = ui->comboBox->findText(username);
            if (index != -1) {
                ui->comboBox->removeItem(index);
            }
            qDebug() << "Account removed successfully.";
        } else {
            qDebug() << "Failed to remove account.";
        }
    });

    // "Connect"
    connect(ui->connectButton, &QPushButton::clicked, this, [this]() {
        QVariantMap userData = getCurrentUserData();
        if (userData.isEmpty()) {
            QMessageBox::warning(this, "Error", "No user data found. Please select a user.");
            return;
        }

        QString server = userData["server"].toString();
        QString email = userData["username"].toString();
        QString password = userData["password"].toString();

        if (server.isEmpty() || email.isEmpty() || password.isEmpty()) {
            QMessageBox::warning(this, "Error", "Incomplete user data. Please check your account details.");
            return;
        }

        // Attempt connection using ConnectionManager
        int port = 993;  // Default IMAP SSL port
        if (connectionManager->connectToServer(server, port, email, password)) {
            qDebug() << "Connection successful. Fetching repositories...";
            QStringList repositories = connectionManager->fetchRepositories();
            populateRepositories(repositories);
        } else {
            QMessageBox::critical(this, "Connection Failed", "Unable to connect to the server. Please check your credentials or server settings.");
            qDebug() << "Connection failed.";
        }
    });

    // "Fetch Sender" => now calls emailManager
    connect(ui->fetchSenderButton, &QPushButton::clicked, this, [this]() {
        QString repo = ui->repositoryComboBox->currentText();
        emailManager->requestFetchSenders(repo);
    });

    // Cleanup button
    connect(ui->runCleanupButton, &QPushButton::clicked, this, [this]() {
        QString repository = connectionManager->getCurrentRepository();
        if (repository.isEmpty()) {
            QMessageBox::critical(this, "Repository Error", "No repository selected. Please select one.");
            return;
        }

        QStringList selectedSenders;
        for (int row = 0; row < ui->senderTableWidget->rowCount(); ++row) {
            QTableWidgetItem *senderItem = ui->senderTableWidget->item(row, 0);
            if (senderItem && senderItem->checkState() == Qt::Checked) {
                selectedSenders.append(senderItem->text());
            }
        }

        if (selectedSenders.isEmpty()) {
            QMessageBox::information(this, "No Selection", "No senders selected for cleanup.");
            return;
        }

        bool deleteRead = ui->deleteReadCheckBox->isChecked();
        bool excludeAttachments = !ui->deleteWithAttach->isChecked();

        emailManager->applyCleanupRules(repository, selectedSenders, deleteRead, excludeAttachments);
    });

    // Update ui after clean
    connect(emailManager, &EmailManager::progressUpdated, this, &MainWindow::updateProgress);

    connect(emailManager, &EmailManager::cleanupCompleted, this, [this]() {
        ui->statusbar->showMessage("Cleanup completed successfully!");
        ui->progressBar->setVisible(false);
    });

    connect(emailManager, &EmailManager::senderDeleted, this, [this](const QString &sender) {
        for (int row = ui->senderTableWidget->rowCount() - 1; row >= 0; --row) {
            QTableWidgetItem *senderItem = ui->senderTableWidget->item(row, 0);
            if (senderItem && senderItem->text() == sender) {
                ui->senderTableWidget->removeRow(row);
                break;
            }
        }
        qDebug() << "Removed sender from table:" << sender;
    });

    connect(emailManager, &EmailManager::removeSenderFromTable, this, [this](const QString &sender) {
        for (int row = 0; row < ui->senderTableWidget->rowCount(); ++row) {
            QTableWidgetItem *senderItem = ui->senderTableWidget->item(row, 0);
            if (senderItem && senderItem->text() == sender) {
                ui->senderTableWidget->removeRow(row);
                break;
            }
        }
    });

    connect(emailManager, &EmailManager::showAlert, this, [this](const QString &title, const QString &message) {
        QMessageBox::critical(this, title, message);
        ui->progressBar->setVisible(true);  // Ensure progress bar remains visible after alert
    });

    connect(connectionManager, &ConnectionManager::showAlert, this, [this](const QString &title, const QString &message) {
        QMessageBox::critical(this, title, message);
    });

}

void MainWindow::onConnectionStatus(bool success)
{
    if (success) {
        ui->statusbar->showMessage("Connected successfully!");
        qDebug() << "Connection successful.";
    } else {
        ui->statusbar->showMessage("Failed to connect to the server.");
        qDebug() << "Connection failed.";
    }
}

void MainWindow::onRepositoriesFetched(const QStringList &repositories)
{
    qDebug() << "Populating repositories into comboBox:" << repositories;
    populateRepositories(repositories);
}

void MainWindow::onCleanupCompleted()
{
    ui->statusbar->showMessage("Cleanup completed!");
}

void MainWindow::setupUserAndRepositorySelection()
{
    auto users = userManager->loadUsers();
    for (const auto &user : users) {
        ui->comboBox->addItem(user["username"].toString());
    }

    // Clear repositoryComboBox when user selection changes
    connect(ui->comboBox, &QComboBox::currentTextChanged, this, [this](const QString &username) {
        if (username.isEmpty()) {
            qDebug() << "No user selected.";
        } else {
            qDebug() << "User changed to:" << username;
        }

        // Clear repositoryComboBox
        //ui->repositoryComboBox->clear();
    });

    // Leave this as is if you want to log the selected repository
    connect(ui->repositoryComboBox, &QComboBox::currentTextChanged, this, [this](const QString &repository) {
        if (repository.isEmpty()) {
            qDebug() << "No repository selected.";
            return;
        }

        connectionManager->setCurrentRepository(repository);
        qDebug() << "Repository selected and set:" << repository;
    });
}

void MainWindow::populateRepositories(const QStringList &repositories)
{
    ui->repositoryComboBox->clear();

    if (repositories.isEmpty()) {
        qDebug() << "No repositories found to populate.";
        return;
    }

    ui->repositoryComboBox->addItems(repositories);
    qDebug() << "Repositories populated into comboBox:" << repositories;
}

void MainWindow::loadUserAccounts()
{
    auto users = userManager->loadUsers();
    for (const auto &user : users) {
        addAccountToTable(user["username"].toString(), user["server"].toString());
    }
}

void MainWindow::addAccountToTable(const QString &username, const QString &server)
{
    int row = ui->accountsTable->rowCount();
    ui->accountsTable->insertRow(row);

    QTableWidgetItem *usernameItem = new QTableWidgetItem(username);
    QTableWidgetItem *serverItem   = new QTableWidgetItem(server);

    ui->accountsTable->setItem(row, 0, usernameItem);
    ui->accountsTable->setItem(row, 1, serverItem);
}

QVariantMap MainWindow::getCurrentUserData()
{
    QString selectedUser = ui->comboBox->currentText();
    if (selectedUser.isEmpty()) {
        qDebug() << "No user selected.";
        return {};
    }

    auto users = userManager->loadUsers();
    for (const auto &usr : users) {
        if (usr["username"].toString() == selectedUser) {
            return usr;
        }
    }

    qDebug() << "User not found in JSON.";
    return {};
}

void MainWindow::updateProgress(int value, const QString &status) {
    ui->progressBar->setValue(value);
    ui->statusbar->showMessage(status);

    if (value == 100 || value == 0) {
        ui->progressBar->setVisible(false);
    } else {
        ui->progressBar->setVisible(true);
    }
}

