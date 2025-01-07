#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QObject>
#include <QSslSocket>
#include <QStringList>
#include <QDebug>
#include <QByteArray>
#include <QRegularExpression>

class ConnectionManager : public QObject {
    Q_OBJECT

public:
    explicit ConnectionManager(QObject *parent = nullptr);
    ~ConnectionManager();

    bool connectToServer(const QString &server, int port, const QString &username, const QString &password);
    void disconnectFromServer();
    bool isConnected() const;

    // Fetch mail folders from the server (IMAP LIST)
    QStringList fetchRepositories();

    QMap<QString, int>fetchSenders(const QString &repository);


public slots:
    void requestFetchSenders(const QString &repository);

signals:
    // Emitted after a successful/failed connection attempt
    void connectionStatus(bool success);

    // Emitted after we fetch the folder list
    void repositoriesFetched(const QStringList &repositories);

    void sendersFetched(QMap<QString, int> senderCounts);

    void progressUpdated(int value, const QString &status);

private:
    QSslSocket *sslSocket;
    bool connected;

    // Reads all pending data from the server (blocking up to 5s)
    QString readResponse();

    // Extract mailbox name from a "* LIST ..." line, then decode if MUTF-7
    QString parseListLineForMailboxName(const QString &line);

    // Decode an IMAP Modified UTF-7 folder name to Unicode
    QString decodeModifiedUTF7(const QString &mutf7Input);

    QString encodeModifiedUTF7(const QString &input);

    QString extractSenderFromLine(const QString &line);

    QString extractSenderFromFetchResponse(const QString &fetchResponse);

};

#endif // CONNECTIONMANAGER_H
