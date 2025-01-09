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
    void showAlert(const QString &title, const QString &message);
    void removeSenderFromTable(const QString &sender);
    void senderDeleted(const QString &sender);

private:
    ConnectionManager *m_conn;  // must not be null if we want real IMAP
    QStringList repositories;
    QString extractSenderFromFetchResponse(const QString &fetchResponse);
    QString decodeMimeEncodedString(const QString &encodedString);
    QString findTrashFolder();

    bool emailHasAttachments(const QString &msgID);
    bool isEmailRead(const QString &msgID);
    QString fetchSenderForMessage(const QString &msgID);
    QStringList extractMessageIDsFromResponse(const QString &response);
    QStringList findSendersByMessageID(const QString &msgID, const QStringList &selectedSenders);

};

#endif // EMAILMANAGER_H
