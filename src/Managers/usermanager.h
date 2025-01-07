#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QList>
#include <QVariantMap>

class UserManager : public QObject
{
    Q_OBJECT

public:
    explicit UserManager(QObject *parent = nullptr);

    bool addUser(const QString &server, const QString &username, const QString &password);
    bool removeUser(const QString &username);
    QList<QVariantMap> loadUsers();   // Load users from file
    void saveUsers(const QList<QVariantMap> &users); // Save users to file

private:
    QList<QVariantMap> users; // Store user information
    QString getUsersFilePath();
};

#endif // USERMANAGER_H
