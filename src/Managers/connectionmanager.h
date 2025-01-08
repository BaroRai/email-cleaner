#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include <QSslSocket>
#include <QStringList>
#include <QMap>

/**
 * @class ConnectionManager
 * @brief Manages the IMAP connection, including connect/disconnect,
 *        maintaining a 'connected' flag, listing folders, and providing
 *        public methods to send/read raw IMAP commands.
 */
class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager();

    bool connectToServer(const QString &server, int port,
                         const QString &username, const QString &password);

    void disconnectFromServer();
    bool isConnected() const;

    // Fetch the list of IMAP folders
    QStringList fetchRepositories();

    // Public methods for sending an IMAP command and reading the response
    bool sendCommand(const QString &command);
    QString readResponse(int timeoutMs = 5000);

    QString decodeModifiedUTF7(const QString &input);
    QString encodeModifiedUTF7(const QString &input);

signals:
    void connectionStatus(bool success);
    void repositoriesFetched(const QStringList &repositories);

private:
    QSslSocket *sslSocket;
    bool connected; // The single source of truth about connectivity

    QString parseListLineForMailboxName(const QString &line);

};

#endif // CONNECTIONMANAGER_H
