#include "ConnectionManager.h"

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent),
    sslSocket(new QSslSocket(this)),
    connected(false)
{
    qDebug() << "SSL Support:" << QSslSocket::supportsSsl();
}

ConnectionManager::~ConnectionManager()
{
    disconnectFromServer();
    // sslSocket is auto-deleted since it's a child of this
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
    if (!connected) {
        qDebug() << "Not connected. Can't fetch repositories.";
        return {};
    }

    sslSocket->write("A001 LIST \"\" *\r\n");
    sslSocket->flush();

    QString response = readResponse();
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
                repositories << mailbox;
            }
        }
    }

    qDebug() << "Populating repositories into comboBox:" << repositories;
    emit repositoriesFetched(repositories);
    return repositories;
}

QString ConnectionManager::readResponse()
{
    if (!sslSocket->waitForReadyRead(5000)) {
        qDebug() << "No response from server.";
        return QString();
    }
    QByteArray data = sslSocket->readAll();
    qDebug() << "Server response:" << data;
    return QString(data);
}

QString ConnectionManager::parseListLineForMailboxName(const QString &line)
{
    // E.g.  * LIST (\NoInferiors \HasNoChildren) "/" "Hromadn&AOE-"
    // 1) Skip flags
    int parenPos = line.indexOf(')');
    if (parenPos == -1)
        return QString();

    QString rest = line.mid(parenPos + 1).trimmed();

    // 2) Skip the delimiter token
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

    // 3) Extract the mailbox name
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

    // Decode MUTF-7 (**forced little-endian** hack)
    return decodeModifiedUTF7(mailbox);
}

/**
 * @brief ConnectionManager::decodeModifiedUTF7
 *        Many servers use MUTF-7 for diacritics, but some incorrectly send it
 *        as little-endian. So we decode base64, then flip bytes in pairs.
 */
QString ConnectionManager::decodeModifiedUTF7(const QString &mutf7Input)
{
    QString decoded;
    int i = 0;
    while (i < mutf7Input.size()) {
        if (mutf7Input[i] == QLatin1Char('&')) {
            // check "&-"
            if ((i + 1 < mutf7Input.size()) && mutf7Input[i + 1] == QLatin1Char('-')) {
                decoded.append('&');
                i += 2;
                continue;
            }
            // otherwise parse until '-'
            int start = i + 1;
            int end   = mutf7Input.indexOf('-', start);
            if (end == -1) {
                end = mutf7Input.size(); // malformed
            }
            QString base64chunk = mutf7Input.mid(start, end - start);
            base64chunk.replace(',', '/'); // revert to std base64

            // decode from base64
            QByteArray rawBytes = QByteArray::fromBase64(base64chunk.toLatin1());
            // *** Force-little-endian approach: swap each pair of bytes
            QByteArray swapped;
            swapped.reserve(rawBytes.size());

            for (int b = 0; b < rawBytes.size(); b += 2) {
                char c1 = rawBytes.at(b);
                char c2 = 0;
                if (b + 1 < rawBytes.size()) {
                    c2 = rawBytes.at(b + 1);
                }
                // swap order to interpret as LE
                swapped.append(c2);
                swapped.append(c1);
            }

            // Now interpret swapped as big-endian UTF-16 => yields correct chars
            const ushort *utf16 = reinterpret_cast<const ushort*>(swapped.constData());
            int length = swapped.size() / 2;
            decoded.append(QString::fromUtf16(utf16, length));

            i = (end < mutf7Input.size()) ? (end + 1) : end;
        } else {
            decoded.append(mutf7Input[i]);
            i++;
        }
    }
    return decoded;
}

void ConnectionManager::requestFetchSenders(const QString &repository) {
    QMap<QString, int> senderCounts = fetchSenders(repository);
    emit sendersFetched(senderCounts); // Emit signal with fetched data
}

QMap<QString, int> ConnectionManager::fetchSenders(const QString &repository)
{
    QMap<QString, int> senderCounts;

    if (!connected) {
        emit progressUpdated(0, "Not connected. Cannot fetch senders.");
        return senderCounts;
    }

    QString encodedName = encodeModifiedUTF7(repository);
    emit progressUpdated(0, "Selecting repository: " + repository);

    QString selectCmd = QString("A002 SELECT \"%1\"\r\n").arg(encodedName);
    sslSocket->write(selectCmd.toUtf8());
    sslSocket->flush();

    QString response = readResponse();
    if (!response.contains("A002 OK")) {
        emit progressUpdated(0, "Failed to select repository.");
        return senderCounts;
    }

    emit progressUpdated(10, "Repository selected. Searching emails...");

    sslSocket->write("A003 SEARCH ALL\r\n");
    sslSocket->flush();

    response = readResponse();
    if (response.isEmpty()) {
        emit progressUpdated(0, "No response for SEARCH command.");
        return senderCounts;
    }

    QStringList lines = response.split("\r\n", Qt::SkipEmptyParts);
    QString messageIDsLine;
    for (const QString &ln : lines) {
        if (ln.startsWith("* SEARCH ")) {
            messageIDsLine = ln.mid(8).trimmed();
            break;
        }
    }

    if (messageIDsLine.isEmpty()) {
        emit progressUpdated(0, "No messages found.");
        return senderCounts;
    }

    QStringList msgIDs = messageIDsLine.split(' ', Qt::SkipEmptyParts);
    if (msgIDs.isEmpty()) {
        emit progressUpdated(0, "SEARCH found no message IDs.");
        return senderCounts;
    }

    int totalMessages = msgIDs.size();
    int processedMessages = 0;

    for (const QString &id : msgIDs) {
        QString fetchCmd = QString("A004 FETCH %1 (BODY[HEADER.FIELDS (FROM)])\r\n").arg(id);
        sslSocket->write(fetchCmd.toUtf8());
        sslSocket->flush();

        QString fetchResponse = readResponse();
        if (fetchResponse.isEmpty()) {
            continue;
        }

        QString fromEmail = extractSenderFromFetchResponse(fetchResponse);
        if (!fromEmail.isEmpty()) {
            senderCounts[fromEmail]++;
        }

        processedMessages++;
        int progress = (processedMessages * 100) / totalMessages;
        emit progressUpdated(progress, "Fetching sender information...");
    }

    emit progressUpdated(100, "Sender fetch completed.");
    return senderCounts;
}

QString ConnectionManager::extractSenderFromLine(const QString &line) {
    QByteArray byteArray = line.toUtf8(); // Convert line to QByteArray for efficient parsing
    int startIndex = byteArray.indexOf("<"); // Look for the start of an email address
    int endIndex = byteArray.indexOf(">", startIndex); // Look for the end of the email address

    if (startIndex != -1 && endIndex != -1 && endIndex > startIndex) {
        // Extract the email address between '<' and '>'
        QByteArray email = byteArray.mid(startIndex + 1, endIndex - startIndex - 1);
        return QString::fromUtf8(email); // Convert back to QString
    }

    // If no email address found, return an empty string
    return QString();
}

QString ConnectionManager::encodeModifiedUTF7(const QString &input)
{
    QString encoded;
    QByteArray buffer;
    bool inBase64 = false;

    auto flushBase64 = [&](bool closeBase64) {
        if (!buffer.isEmpty()) {
            // buffer contains 2-byte (UTF-16) chars in host endianness (likely LE).
            // Convert each ushort to big-endian before base64:
            QByteArray bigEndianData;
            bigEndianData.reserve(buffer.size());
            for (int i = 0; i < buffer.size(); i += 2) {
                char low  = buffer[i];
                char high = (i + 1 < buffer.size()) ? buffer[i + 1] : 0;
                // swap to produce (high, low) => big-endian
                bigEndianData.append(high);
                bigEndianData.append(low);
            }

            QByteArray b64 = bigEndianData.toBase64();
            b64.replace('/', ','); // MUTF-7 uses ',' instead of '/'

            // ---- Remove trailing '='
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

        // ASCII range (printable) except '&'
        if (code >= 0x20 && code <= 0x7E && c != QLatin1Char('&')) {
            // If we were in base64 mode, flush
            if (inBase64) {
                flushBase64(true);
            }
            encoded.append(c);
        }
        else if (c == QLatin1Char('&')) {
            // For literal '&', the representation is "&-"
            if (inBase64) {
                flushBase64(true);
            }
            encoded.append("&-");
        }
        else {
            // non-ASCII => go to base64 buffer
            if (!inBase64) {
                inBase64 = true;
            }
            // Append the 2-byte representation (host-endian)
            buffer.append(reinterpret_cast<const char*>(&code), 2);
        }
    }

    // flush leftover
    if (inBase64) {
        flushBase64(true);
    }

    return encoded;
}


QString ConnectionManager::extractSenderFromFetchResponse(const QString &fetchResponse)
{
    // Typical lines might be:
    // * 1 FETCH (BODY[HEADER.FIELDS (FROM)] {42}
    // From: "John Doe" <john@doe.com>
    // ...
    // ) A004 OK FETCH completed
    // We'll split by lines and look for something that starts with "From:"

    QStringList lines = fetchResponse.split("\r\n", Qt::SkipEmptyParts);
    for (const QString &ln : lines) {
        QString lineLower = ln.toLower();
        if (lineLower.startsWith("from:")) {
            // e.g. "From: "John Doe" <john@doe.com>"
            // Let’s do a quick extraction of <...> or fallback to the whole line
            int start = ln.indexOf('<');
            int end   = ln.indexOf('>', start);
            if (start != -1 && end != -1 && end > start) {
                return ln.mid(start + 1, end - start - 1).trimmed();
            } else {
                // fallback: remove "From:" part, return the rest
                return ln.mid(5).trimmed();
            }
        }
    }

    return QString();
}







