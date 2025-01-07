#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

// Forward declarations for managers
class EmailManager;
class UserManager;
class ConnectionManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief The MainWindow class
 *        Our primary GUI window that handles user accounts,
 *        connecting to the IMAP server, listing mailboxes, etc.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Called when we get a connection status signal (true = success, false = fail)
    void onConnectionStatus(bool success);

    // Called when ConnectionManager emits repositoriesFetched(...)
    void onRepositoriesFetched(const QStringList &repositories);

    // Example leftover from your code
    void onCleanupCompleted();

    void updateProgress(int value, const QString &status);

private:
    Ui::MainWindow *ui;
    EmailManager *emailManager;
    UserManager *userManager;
    ConnectionManager *connectionManager;

    void setupThemeComboBox();
    void applyTheme(const QString &theme);
    void setupConnections();
    void loadUserAccounts();
    void addAccountToTable(const QString &username, const QString &server);
    void setupUserAndRepositorySelection();
    void populateRepositories(const QStringList &repositories);
    void fetchSenders();

    // Utility to get the currently selected user from the combo box and load credentials
    QVariantMap getCurrentUserData();
};

#endif // MAINWINDOW_H
