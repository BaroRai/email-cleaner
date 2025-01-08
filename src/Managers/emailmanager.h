#ifndef EMAILMANAGER_H
#define EMAILMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>

class ConnectionManager;

class EmailManager : public QObject
{
    Q_OBJECT

public:
    // We pass a pointer to ConnectionManager in constructor (no stubs)
    explicit EmailManager(ConnectionManager *connMgr, QObject *parent = nullptr);

    QStringList fetchRepositories();
    void applyCleanupRules(const QString &repository, const QStringList &selectedSenders, bool deleteRead, bool excludeAttachments);
    void requestFetchSenders(const QString &repository);
    QMap<QString,int> fetchSenders(const QString &repository);

signals:
    void repositoriesFetched(const QStringList &repositories);
    void cleanupCompleted();
    void progressUpdated(int value, const QString &status);
    void sendersFetched(const QMap<QString,int> &senderCounts);

private:
    ConnectionManager *m_conn;  // must not be null if we want real IMAP
    QStringList repositories;
    QString extractSenderFromFetchResponse(const QString &fetchResponse);
    QString decodeMimeEncodedString(const QString &encodedString);
    QString findTrashFolder();

};

#endif // EMAILMANAGER_H
