#ifndef EMAILMANAGER_H
#define EMAILMANAGER_H

#include <QObject>
#include <QStringList>
#include <QMap>

class ConnectionManager;

class EmailManager : public QObject
{
    Q_OBJECT
public:
    explicit EmailManager(ConnectionManager *connMgr, QObject *parent = nullptr);

    // Basic cleanup logic
    void applyCleanupRules(const QString &repository,
                           const QStringList &selectedSenders,
                           bool deleteRead,
                           bool excludeAttachments);

    QStringList fetchRepositories();

    // To fetch all senders from a given folder
    QMap<QString,int> fetchSenders(const QString &repository);
    void requestFetchSenders(const QString &repository);

signals:
    void repositoriesFetched(const QStringList &repos);
    void showAlert(const QString &title, const QString &message);
    void progressUpdated(int progressValue, const QString &statusMessage);
    void cleanupCompleted();
    void sendersFetched(const QMap<QString,int> &senderCounts);
    void senderDeleted(const QString &senderEmail);
    void removeSenderFromTable(const QString &sender);

private:
    bool isEmailRead(const QString &msgID);
    bool emailHasAttachments(const QString &msgID);
    QStringList findSendersByMessageID(const QString &msgID, const QStringList &selectedSenders);
    QStringList extractMessageIDsFromResponse(const QString &response);
    QString fetchSenderForMessage(const QString &msgID);
    QString extractSenderFromFetchResponse(const QString &fetchResp);
    QString decodeMimeEncodedString(const QString &encodedString);

    ConnectionManager *m_conn;
    QStringList repositories;
};

#endif // EMAILMANAGER_H
