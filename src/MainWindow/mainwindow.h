#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QStringList>

class EmailManager;
class UserManager;
class ConnectionManager;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectionStatus(bool success);
    void onRepositoriesFetched(const QStringList &repositories);
    void onCleanupCompleted();
    void updateProgress(int value, const QString &status);

private:
    Ui::MainWindow *ui;

    // Managers
    ConnectionManager *connectionManager;
    EmailManager      *emailManager;
    UserManager       *userManager;


    void setupThemeComboBox();
    void applyTheme(const QString &theme);
    void setupConnections();
    void loadUserAccounts();
    void addAccountToTable(const QString &username, const QString &server);
    void setupUserAndRepositorySelection();
    void populateRepositories(const QStringList &repositories);
    QVariantMap getCurrentUserData();
    bool cleanupInProgress = false;
};

#endif // MAINWINDOW_H
