#include "mainwindow.h"
#include "src/MainWindow/ui_mainwindow.h"

#include "AddAccountDialog.h"      // If you have this dialog
#include "../Managers/ConnectionManager.h"
#include "../Managers/EmailManager.h"
#include "../Managers/UserManager.h"

#include <QFile>
#include <QSettings>
#include <QDebug>
#include <QDir>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , emailManager(new EmailManager(this))
    , userManager(new UserManager(this))
    , connectionManager(new ConnectionManager(this))
{
    ui->setupUi(this);

    setupThemeComboBox();
    setupConnections();
    loadUserAccounts();

    // Table for listing user accounts
    ui->accountsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->accountsTable->setHorizontalHeaderLabels({"Username", "E-Mail"});

    // Table for listing emails
    ui->senderTableWidget->setColumnCount(2);
    ui->senderTableWidget->setHorizontalHeaderLabels({"Sender (Email)", "Count"});
    ui->senderTableWidget->horizontalHeader()->setStretchLastSection(false);
    ui->senderTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->senderTableWidget->setColumnWidth(1, 80);

    ui->progressBar->setRange(0, 100);  // Set range
    ui->progressBar->setValue(0);      // Initial value
    ui->progressBar->setVisible(false);  // Hide by default

    setupUserAndRepositorySelection();
}

MainWindow::~MainWindow()
{
    delete ui;
}

/**
 * @brief MainWindow::setupThemeComboBox
 *        Example method for handling light/dark theme selection.
 */
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

/**
 * @brief MainWindow::applyTheme
 *        Loads a .qss file to switch between Light/Dark styles.
 */
void MainWindow::applyTheme(const QString &theme)
{
    QDir baseDir(QCoreApplication::applicationDirPath());
    // Adjust for your actual filesystem if needed
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

    // Save the selected theme
    QSettings settings("YourCompany", "YourApp");
    settings.setValue("theme", theme);
}

/**
 * @brief MainWindow::setupConnections
 *        Connect signals/slots for UI elements, connection manager, etc.
 */
void MainWindow::setupConnections()
{
    // ConnectionManager signals
    connect(connectionManager, &ConnectionManager::connectionStatus,
            this, &MainWindow::onConnectionStatus);
    connect(connectionManager, &ConnectionManager::repositoriesFetched,
            this, &MainWindow::onRepositoriesFetched);

    // Example: EmailManager signals if you have them
    connect(emailManager, &EmailManager::connectionStatus,
            this, &MainWindow::onConnectionStatus);
    connect(emailManager, &EmailManager::cleanupCompleted,
            this, &MainWindow::onCleanupCompleted);

    // "Add Account" button
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

    // "Remove Account" button
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

    // "Connect to mail server" button
    connect(ui->connectButton, &QPushButton::clicked, this, [this]() {
        QVariantMap userData = getCurrentUserData();
        if (userData.isEmpty()) {
            qDebug() << "No user data found.";
            return;
        }

        QString server   = userData["server"].toString();
        QString username = userData["username"].toString();
        QString password = userData["password"].toString();

        // Connect & fetch repos
        if (connectionManager->connectToServer(server, 993, username, password)) {
            qDebug() << "Connection successful, now fetching repositories.";
            QStringList repositories = connectionManager->fetchRepositories();
            populateRepositories(repositories);
        } else {
            qDebug() << "Connection failed.";
        }
    });

    // Connect fetchSenderButton to request fetch senders
    connect(ui->fetchSenderButton, &QPushButton::clicked, this, [this]() {
        QString repository = ui->repositoryComboBox->currentText();
        if (!repository.isEmpty()) {
            connectionManager->requestFetchSenders(repository);
        }
    });

    // Connect signal to handle fetched senders
    connect(connectionManager, &ConnectionManager::sendersFetched, this, [this](const QMap<QString, int> &senders) {
        ui->senderTableWidget->clearContents();
        ui->senderTableWidget->setRowCount(0);

        int row = 0;
        for (auto it = senders.begin(); it != senders.end(); ++it) {
            ui->senderTableWidget->insertRow(row);

            QTableWidgetItem *senderItem = new QTableWidgetItem(it.key());
            senderItem->setCheckState(Qt::Unchecked); // Allow selection
            QTableWidgetItem *countItem = new QTableWidgetItem(QString::number(it.value()));

            ui->senderTableWidget->setItem(row, 0, senderItem);
            ui->senderTableWidget->setItem(row, 1, countItem);

            ++row;
        }

        qDebug() << "Senders populated in senderTableWidget.";
    });

    // Status bar update
    connect(connectionManager, &ConnectionManager::progressUpdated, this, &MainWindow::updateProgress);

}

/**
 * @brief MainWindow::onConnectionStatus
 *        Slot to handle connection status updates.
 * @param success true if connected/logged in, false otherwise
 */
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

/**
 * @brief MainWindow::onRepositoriesFetched
 *        Slot triggered after fetchRepositories() is done.
 * @param repositories the list of folder names
 */
void MainWindow::onRepositoriesFetched(const QStringList &repositories)
{
    qDebug() << "Populating repositories into comboBox:" << repositories;
    populateRepositories(repositories);
}

/**
 * @brief MainWindow::onCleanupCompleted
 *        Example leftover slot if you do cleanup in EmailManager
 */
void MainWindow::onCleanupCompleted()
{
    ui->statusbar->showMessage("Cleanup completed!");
}

/**
 * @brief MainWindow::setupUserAndRepositorySelection
 *        Load usernames into the comboBox. Also handle user changes -> disconnect.
 */
void MainWindow::setupUserAndRepositorySelection() {
    // Populate the user comboBox with usernames
    auto users = userManager->loadUsers();
    for (const auto &user : users) {
        ui->comboBox->addItem(user["username"].toString());
    }

    // Connect user selection to repository fetching
    connect(ui->comboBox, &QComboBox::currentTextChanged, this, [this](const QString &username) {
        if (username.isEmpty()) {
            qDebug() << "No user selected.";
            ui->repositoryComboBox->clear(); // Clear the repository comboBox
            return;
        }

        QVariantMap userData = getCurrentUserData();
        if (userData.isEmpty()) {
            qDebug() << "Failed to fetch user data for:" << username;
            ui->repositoryComboBox->clear(); // Clear the repository comboBox
            return;
        }

        QString server = userData["server"].toString();
        QString user = userData["username"].toString();
        QString password = userData["password"].toString();

        // Disconnect and clear repositories if a new user is selected
        connectionManager->disconnectFromServer();
        ui->repositoryComboBox->clear();

        if (connectionManager->connectToServer(server, 993, user, password)) {
            QStringList repositories = connectionManager->fetchRepositories();
            ui->repositoryComboBox->clear(); // Clear old items
            ui->repositoryComboBox->addItems(repositories);
            qDebug() << "Repositories for user" << username << ":" << repositories;
        }
    });

    // Debugging: Log selected repository
    connect(ui->repositoryComboBox, &QComboBox::currentTextChanged, this, [](const QString &repository) {
        qDebug() << "Selected repository:" << repository;
    });
}

/**
 * @brief MainWindow::populateRepositories
 *        Clear the repository comboBox and add the new list.
 */
void MainWindow::populateRepositories(const QStringList &repositories) {
    ui->repositoryComboBox->clear();
    ui->repositoryComboBox->addItems(repositories);
    qDebug() << "Repositories populated:" << repositories;
}

/**
 * @brief MainWindow::loadUserAccounts
 *        Loads user info from the user manager and populates the accounts table.
 */
void MainWindow::loadUserAccounts() {
    auto users = userManager->loadUsers();
    for (const auto &user : users) {
        addAccountToTable(user["username"].toString(), user["server"].toString());
    }
}

/**
 * @brief MainWindow::addAccountToTable
 *        Utility to append a row to the accountsTable with username/server.
 */
void MainWindow::addAccountToTable(const QString &username, const QString &server) {
    int row = ui->accountsTable->rowCount();
    ui->accountsTable->insertRow(row);

    QTableWidgetItem *usernameItem = new QTableWidgetItem(username);
    QTableWidgetItem *serverItem   = new QTableWidgetItem(server);

    ui->accountsTable->setItem(row, 0, usernameItem);
    ui->accountsTable->setItem(row, 1, serverItem);
}

/**
 * @brief MainWindow::getCurrentUserData
 *        Helper to fetch the server/username/password from stored JSON, matching the current comboBox user.
 */
QVariantMap MainWindow::getCurrentUserData() {
    QString selectedUser = ui->comboBox->currentText();
    if (selectedUser.isEmpty()) {
        qDebug() << "No user selected.";
        return {};
    }

    auto users = userManager->loadUsers();
    for (const auto &user : users) {
        if (user["username"].toString() == selectedUser) {
            return user; // e.g. { "username":..., "server":..., "password":... }
        }
    }

    qDebug() << "User not found in JSON.";
    return {};
}

void MainWindow::updateProgress(int value, const QString &status) {
    ui->progressBar->setValue(value);
    ui->statusbar->showMessage(status);

    if (value == 100) {
        ui->progressBar->setVisible(false);  // Hide progress bar on completion
    } else {
        ui->progressBar->setVisible(true);   // Show progress bar during updates
    }
}







