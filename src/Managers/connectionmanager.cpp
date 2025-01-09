#include "ConnectionManager.h"
#include <QDebug>
#include <QByteArray>

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent),
    sslSocket(new QSslSocket(this)),
    connected(false),
    currentRepository("")
{
    qDebug() << "SSL Support:" << QSslSocket::supportsSsl();
}

ConnectionManager::~ConnectionManager()
{
    disconnectFromServer();
}

bool ConnectionManager::connectToServer(const QString &server, int port, const QString &username, const QString &password)
{
    // If already connected, disconnect and fully abort first
    if (connected) {
        disconnectFromServer();
    } else {
        // If the socket is in some weird state after a failed attempt, abort it
        sslSocket->abort();
    }

    sslSocket->connectToHostEncrypted(server, port);
    if (!sslSocket->waitForConnected(5000)) {
        qDebug() << "Failed to connect to server:" << sslSocket->errorString();
        emit connectionStatus(false);
        connected = false;
        return false;
    }

    qDebug() << "Connected to server:" << server;
    // Read server greeting
    QString greeting = readResponse();
    qDebug() << "Server greeting:" << greeting;
    if (!greeting.contains("* OK")) {
        qDebug() << "Unexpected server greeting.";
        emit connectionStatus(false);
        connected = false;
        return false;
    }

    // Login
    QString loginCmd = QString("1 LOGIN %1 %2\r\n").arg(username, password);
    sslSocket->write(loginCmd.toUtf8());
    sslSocket->flush();

    QString loginResp = readResponse();
    qDebug() << "Login response:" << loginResp;
    if (!loginResp.contains("1 OK")) {
        qDebug() << "Login failed:" << loginResp;
        emit connectionStatus(false);
        connected = false;
        return false;
    }

    qDebug() << "Login successful.";
    connected = true;
    emit connectionStatus(true);
    return true;
}

void ConnectionManager::disconnectFromServer()
{
    if (sslSocket && sslSocket->isOpen()) {
        sslSocket->write("A999 LOGOUT\r\n");
        sslSocket->flush();
        sslSocket->disconnectFromHost();
    }
    connected = false;
    emit connectionStatus(false);
}

bool ConnectionManager::isConnected() const
{
    return connected;
}

QStringList ConnectionManager::fetchRepositories()
{
    qDebug() << "Fetching repositories...";
    if (!connected) {
        qDebug() << "Not connected. Can't fetch repositories.";
        return {};
    }

    sslSocket->write("A001 LIST \"\" *\r\n");
    sslSocket->flush();

    QString response = readResponse();
    qDebug() << "Server LIST response:" << response;
    if (response.isEmpty()) {
        qDebug() << "No response from server when listing folders.";
        return {};
    }

    QStringList repositories;
    QStringList lines = response.split("\r\n", Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith("* LIST")) {
            QString mailbox = parseListLineForMailboxName(line);
            if (!mailbox.isEmpty()) {
                qDebug() << "Decoded repository name:" << mailbox;
                repositories << mailbox;
            }
        }
    }

    qDebug() << "Final repositories list:" << repositories;
    emit repositoriesFetched(repositories);
    return repositories;
}


QString ConnectionManager::parseListLineForMailboxName(const QString &line)
{
    int parenPos = line.indexOf(')');
    if (parenPos == -1)
        return QString();

    QString rest = line.mid(parenPos + 1).trimmed();

    auto skipOneToken = [&](QString &s) {
        s = s.trimmed();
        if (s.isEmpty()) return;

        if (s.startsWith('"')) {
            int quoteEnd = s.indexOf('"', 1);
            if (quoteEnd != -1)
                s.remove(0, quoteEnd + 1);
            else
                s.clear();
        } else {
            int spacePos = s.indexOf(' ');
            if (spacePos != -1)
                s.remove(0, spacePos + 1);
            else
                s.clear();
        }
        s = s.trimmed();
    };
    skipOneToken(rest);

    QString mailbox;
    rest = rest.trimmed();
    if (rest.startsWith('"')) {
        int secondQuote = rest.indexOf('"', 1);
        if (secondQuote != -1)
            mailbox = rest.mid(1, secondQuote - 1);
        else
            mailbox = rest.mid(1);
    } else {
        int spacePos = rest.indexOf(' ');
        mailbox = (spacePos == -1) ? rest : rest.left(spacePos);
    }

    mailbox = mailbox.trimmed();
    if (mailbox.isEmpty()) {
        return QString();
    }

    return decodeModifiedUTF7(mailbox);
}

QString ConnectionManager::decodeModifiedUTF7(const QString &mutf7Input)
{
    qDebug() << "Decoding Modified UTF-7 input:" << mutf7Input;

    QString decoded;
    int i = 0;
    while (i < mutf7Input.size()) {
        if (mutf7Input[i] == QLatin1Char('&')) {
            if ((i + 1 < mutf7Input.size()) && mutf7Input[i + 1] == QLatin1Char('-')) {
                decoded.append('&');
                i += 2;
                continue;
            }
            int start = i + 1;
            int end   = mutf7Input.indexOf('-', start);
            if (end == -1) {
                end = mutf7Input.size();
            }
            QString base64chunk = mutf7Input.mid(start, end - start);
            base64chunk.replace(',', '/');

            QByteArray rawBytes = QByteArray::fromBase64(base64chunk.toLatin1());
            QByteArray swapped;
            swapped.reserve(rawBytes.size());

            for (int b = 0; b < rawBytes.size(); b += 2) {
                char c1 = rawBytes.at(b);
                char c2 = (b + 1 < rawBytes.size()) ? rawBytes.at(b + 1) : 0;
                swapped.append(c2);
                swapped.append(c1);
            }

            const ushort *utf16 = reinterpret_cast<const ushort *>(swapped.constData());
            int length = swapped.size() / 2;
            decoded.append(QString::fromUtf16(utf16, length));

            i = (end < mutf7Input.size()) ? (end + 1) : end;
        } else {
            decoded.append(mutf7Input[i]);
            i++;
        }
    }

    qDebug() << "Decoded name:" << decoded;
    return decoded;
}

QString ConnectionManager::encodeModifiedUTF7(const QString &input)
{
    QString encoded;
    QByteArray buffer;
    bool inBase64 = false;

    auto flushBase64 = [&](bool closeBase64) {
        if (!buffer.isEmpty()) {
            QByteArray bigEndianData;
            bigEndianData.reserve(buffer.size());
            for (int i = 0; i < buffer.size(); i += 2) {
                char low  = buffer[i];
                char high = (i + 1 < buffer.size()) ? buffer[i + 1] : 0;
                bigEndianData.append(high);
                bigEndianData.append(low);
            }

            QByteArray b64 = bigEndianData.toBase64();
            b64.replace('/', ',');
            while (!b64.isEmpty() && b64.endsWith('=')) {
                b64.chop(1);
            }
            encoded.append('&' + QString::fromLatin1(b64) + '-');
            buffer.clear();
        }
        if (closeBase64) {
            inBase64 = false;
        }
    };

    for (int i = 0; i < input.size(); ++i) {
        QChar c = input[i];
        ushort code = c.unicode();
        if (code >= 0x20 && code <= 0x7E && c != QLatin1Char('&')) {
            if (inBase64) {
                flushBase64(true);
            }
            encoded.append(c);
        }
        else if (c == QLatin1Char('&')) {
            if (inBase64) {
                flushBase64(true);
            }
            encoded.append("&-");
        }
        else {
            if (!inBase64) {
                inBase64 = true;
            }
            buffer.append(reinterpret_cast<const char*>(&code), 2);
        }
    }
    if (inBase64) {
        flushBase64(true);
    }
    return encoded;
}

bool ConnectionManager::sendCommand(const QString &command)
{
    if (!connected) {
        qDebug() << "ConnectionManager: Not connected. Cannot send command:" << command;
        return false;
    }

    if (command.startsWith("A010 SELECT")) {
        int start = command.indexOf("\"") + 1;
        int end = command.lastIndexOf("\"");
        if (start != -1 && end != -1 && end > start) {
            currentRepository = decodeModifiedUTF7(command.mid(start, end - start));
            qDebug() << "Current repository set to:" << currentRepository;
        }
    }

    qDebug() << "ConnectionManager: Sending command:" << command;
    sslSocket->write(command.toUtf8());
    sslSocket->flush();
    return true;
}

QString ConnectionManager::readResponse(int timeoutMs)
{
    if (!sslSocket->waitForReadyRead(timeoutMs)) {
        qDebug() << "ConnectionManager: No response from server (timeout).";
        return QString();
    }
    QByteArray data = sslSocket->readAll();
    qDebug() << "ConnectionManager: Server response:" << data;
    return QString::fromLatin1(data);
}

void ConnectionManager::setCurrentRepository(const QString &repository)
{
    currentRepository = repository;
    qDebug() << "Current repository updated to:" << currentRepository;
}

QString ConnectionManager::getCurrentRepository() const
{
    if (currentRepository.isEmpty()) {
        qDebug() << "No repository currently selected.";
        return QString();
    }
    qDebug() << "Current repository retrieved as:" << currentRepository;
    return currentRepository;
}

QString ConnectionManager::findTrashFolder()
{
    if (!connected) {
        qDebug() << "Not connected. Cannot find trash folder.";
        return QString();
    }

    QStringList repositories = fetchRepositories();
    if (repositories.isEmpty()) {
        qDebug() << "No repositories found to search for trash folder.";
        return QString();
    }

    // Search for the folder containing "Trash" or its localized equivalents
    QStringList trashKeywords = {"Trash", "Deleted Items", "Bin", "Kôš", "Papierkorb", "Corbeille"};
    for (const QString &repo : repositories) {
        for (const QString &keyword : trashKeywords) {
            if (repo.contains(keyword, Qt::CaseInsensitive)) {
                qDebug() << "Found trash folder:" << repo;
                return repo;
            }
        }
    }

    qDebug() << "No trash folder found.";
    return QString();
}
