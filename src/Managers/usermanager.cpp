#include "usermanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

UserManager::UserManager(QObject *parent) : QObject(parent) {
    users = loadUsers(); // Initialize the users list by loading from JSON
}

QString UserManager::getUsersFilePath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir); // Ensure the directory exists
    return configDir + "/users.json";
}

QList<QVariantMap> UserManager::loadUsers() {
    QString filePath = getUsersFilePath();
    QFile file(filePath);
    qDebug() << "JSON file path:" << filePath; // Debug output
    if (!file.exists()) {
        qDebug() << "users.json not found. Starting with an empty user list.";
        return {};
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open users.json for reading.";
        return {};
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "Invalid JSON structure in users.json.";
        return {};
    }

    QJsonArray usersArray = doc.object().value("accounts").toArray();
    QList<QVariantMap> loadedUsers;
    for (const QJsonValue &value : usersArray) {
        loadedUsers.append(value.toObject().toVariantMap());
    }

    return loadedUsers;
}

void UserManager::saveUsers(const QList<QVariantMap> &users) {
    QJsonArray usersArray;
    for (const QVariantMap &user : users) {
        usersArray.append(QJsonObject::fromVariantMap(user));
    }

    QJsonObject rootObject;
    rootObject["accounts"] = usersArray;

    QString filePath = getUsersFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to open users.json for writing.";
        return;
    }

    file.write(QJsonDocument(rootObject).toJson(QJsonDocument::Indented));
    file.close();
}

bool UserManager::addUser(const QString &server, const QString &username, const QString &password) {
    for (const auto &user : users) {
        if (user["username"].toString() == username) {
            qDebug() << "User already exists with username:" << username;
            return false;
        }
    }

    QVariantMap newUser;
    newUser["server"] = server;
    newUser["username"] = username;
    newUser["password"] = password;
    users.append(newUser);
    saveUsers(users);
    return true;
}

bool UserManager::removeUser(const QString &username) {
    for (int i = 0; i < users.size(); ++i) {
        if (users[i]["username"].toString() == username) {
            users.removeAt(i);
            saveUsers(users);
            qDebug() << "User removed with username:" << username;
            return true;
        }
    }
    qDebug() << "User not found with username:" << username;
    return false;
}

