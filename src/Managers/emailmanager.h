#ifndef EMAILMANAGER_H
#define EMAILMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

class EmailManager : public QObject {
    Q_OBJECT

public:
    explicit EmailManager(QObject *parent = nullptr);

    Q_INVOKABLE bool connectToServer(const QString &server, const QString &username, const QString &password);
    Q_INVOKABLE QStringList fetchRepositories();
    Q_INVOKABLE void applyCleanupRules(bool deleteRead, bool excludeAttachments);

signals:
    void connectionStatus(bool success);
    void repositoriesFetched(const QStringList &repositories);
    void cleanupCompleted();

private:
    QStringList repositories;
};

#endif // EMAILMANAGER_H
