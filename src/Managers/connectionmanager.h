#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include <QSslSocket>
#include <QVariantMap>
#include <QStringList>

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager();

    // Connects to the mail server
    bool connectToServer(const QString &server, int port, const QString &username, const QString &password);

    // Disconnects from the mail server
    void disconnectFromServer();

    // Checks if the connection is established
    bool isConnected() const;

    // Fetches the list of repositories (mail folders)
    QStringList fetchRepositories();

    // Returns the currently selected repository (mail folder)
    QString getCurrentRepository() const;

    // Sets the current repository (mail folder)
    void setCurrentRepository(const QString &repository);

    // Finds the trash folder among the repositories
    QString findTrashFolder();

    QString decodeModifiedUTF7(const QString &mutf7Input);

    // Encodes a regular UTF-8 string into Modified UTF-7 format
    QString encodeModifiedUTF7(const QString &input);

    QString readResponse(int timeoutMs = 5000);

    void sendCommand(const QString &command);

signals:
    // Signals connection status (true/false)
    void connectionStatus(bool success);

    // Signals that repositories were fetched successfully
    void repositoriesFetched(const QStringList &repositories);

    // Signals progress update (value, status)
    void progressUpdated(int value, const QString &status);

    // Signals alert message to be displayed
    void showAlert(const QString &title, const QString &message);

private:
    // Parses a line from LIST response to extract the mailbox name
    QString parseListLineForMailboxName(const QString &line);

    // Decodes Modified UTF-7 encoded string to a regular UTF-8 string
    QSslSocket *sslSocket;  // SSL Socket for secure communication
    bool connected;         // Connection status flag
    QString currentRepository;  // The currently selected repository (folder)
};

#endif // CONNECTIONMANAGER_H
