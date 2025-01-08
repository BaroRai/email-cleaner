#include "EmailManager.h"
#include "../Managers/ConnectionManager.h"
#include <QDebug>
#include <qregularexpression.h>

EmailManager::EmailManager(ConnectionManager *connMgr, QObject *parent)
    : QObject(parent),
    m_conn(connMgr)
{
    // We rely on m_conn->isConnected() to be true before fetchSenders
}

QStringList EmailManager::fetchRepositories()
{
    if (m_conn && m_conn->isConnected()) {
        // actual listing
        QStringList repos = m_conn->fetchRepositories();
        emit repositoriesFetched(repos);
        return repos;
    } else {
        // fallback or empty
        repositories = {"Inbox", "Sent", "Trash"};
        emit repositoriesFetched(repositories);
        return repositories;
    }
}

void EmailManager::applyCleanupRules(bool deleteRead, bool excludeAttachments)
{
    Q_UNUSED(deleteRead);
    Q_UNUSED(excludeAttachments);
    emit cleanupCompleted();
}

void EmailManager::requestFetchSenders(const QString &repository)
{
    // calls the real logic
    QMap<QString,int> result = fetchSenders(repository);
    Q_UNUSED(result);
}

QMap<QString,int> EmailManager::fetchSenders(const QString &repository)
{
    QMap<QString,int> senderCounts;
    if (!m_conn) {
        qDebug() << "EmailManager: No ConnectionManager set. Cannot fetch senders.";
        emit progressUpdated(0, "Connection manager is null.");
        return senderCounts;
    }

    if (!m_conn->isConnected()) {
        qDebug() << "EmailManager: Not connected.";
        emit progressUpdated(0, "Not connected to server.");
        return senderCounts;
    }

    // 1) SELECT folder
    emit progressUpdated(0, "Selecting repository: " + repository);

    QString encoded = m_conn->encodeModifiedUTF7(repository);
    QString selectCmd = QString("A002 SELECT \"%1\"\r\n").arg(encoded);
    m_conn->sendCommand(selectCmd);

    QString response = m_conn->readResponse();
    if (!response.contains("A002 OK")) {
        qDebug() << "Failed to select folder. Response:" << response;
        emit progressUpdated(0, "Failed to SELECT folder: " + repository);
        return senderCounts;
    }

    // 2) SEARCH ALL
    emit progressUpdated(10, "Searching emails in: " + repository);
    m_conn->sendCommand("A003 SEARCH ALL\r\n");
    response = m_conn->readResponse();

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
        emit progressUpdated(0, "No messages found in: " + repository);
        return senderCounts;
    }

    QStringList msgIDs = messageIDsLine.split(' ', Qt::SkipEmptyParts);
    if (msgIDs.isEmpty()) {
        emit progressUpdated(0, "SEARCH found no message IDs.");
        return senderCounts;
    }

    int total = msgIDs.size();
    int processed = 0;

    // 3) FETCH FROM header for each ID
    for (const QString &id : msgIDs) {
        QString fetchCmd = QString("A004 FETCH %1 (BODY[HEADER.FIELDS (FROM)])\r\n").arg(id);
        m_conn->sendCommand(fetchCmd);

        QString fetchResp = m_conn->readResponse();
        if (fetchResp.isEmpty()) {
            continue;
        }

        QString fromEmail = extractSenderFromFetchResponse(fetchResp);
        if (!fromEmail.isEmpty()) {
            senderCounts[fromEmail]++;
        }

        processed++;
        int progress = (processed * 100) / total;
        emit progressUpdated(progress, "Fetching sender info...");
    }

    // 4) Done
    emit progressUpdated(100, "Sender fetch completed.");
    emit sendersFetched(senderCounts);

    return senderCounts;
}

QString EmailManager::extractSenderFromFetchResponse(const QString &fetchResp)
{
    QStringList lines = fetchResp.split("\r\n", Qt::SkipEmptyParts);
    for (QString ln : lines) {
        ln = ln.trimmed();

        if (ln.startsWith("From:", Qt::CaseInsensitive)) {
            ln = ln.mid(5).trimmed();  // Remove "From:"

            QString email, displayName;

            // Extract email address
            int start = ln.indexOf('<');
            int end = ln.indexOf('>', start);
            if (start != -1 && end != -1 && end > start) {
                email = ln.mid(start + 1, end - start - 1).trimmed();
            }

            // Extract display name (if available)
            int quoteStart = ln.indexOf('"');
            int quoteEnd = ln.indexOf('"', quoteStart + 1);
            if (quoteStart != -1 && quoteEnd != -1 && quoteEnd > quoteStart) {
                displayName = ln.mid(quoteStart + 1, quoteEnd - quoteStart - 1).trimmed();
            } else if (start != -1) {
                displayName = ln.left(start).trimmed();  // Fallback: Text before '<'
            }

            // Decode MIME-encoded display name if applicable
            if (!displayName.isEmpty() && displayName.contains("=?")) {
                displayName = decodeMimeEncodedString(displayName);
            }

            // Return combined "Display Name <email@example.com>" format
            if (!displayName.isEmpty() && !email.isEmpty()) {
                return QString("%1 <%2>").arg(displayName, email);
            } else if (!email.isEmpty()) {
                return email;
            }
        }
    }

    return QString();  // Return empty if no valid sender is found
}

QString EmailManager::decodeMimeEncodedString(const QString &encodedString)
{
    QString decoded;
    QRegularExpression mimePattern(R"(^=\?([^?]+)\?([QBqb])\?(.+)\?=$)");
    QRegularExpressionMatchIterator it = mimePattern.globalMatch(encodedString);

    if (encodedString.contains("=?")) {
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (match.hasMatch()) {
                QString charset = match.captured(1);  // Character set (e.g., "UTF-8")
                QString encoding = match.captured(2).toUpper();  // Encoding type (Q or B)
                QString encodedText = match.captured(3);

                QByteArray decodedBytes;
                if (encoding == "B") {
                    // Decode Base64
                    decodedBytes = QByteArray::fromBase64(encodedText.toUtf8());
                } else if (encoding == "Q") {
                    // Decode Quoted-Printable
                    QByteArray result;
                    QByteArray encodedBytes = encodedText.toUtf8();
                    encodedBytes.replace('_', ' ');  // Replace underscores with spaces

                    for (int i = 0; i < encodedBytes.size(); ++i) {
                        if (encodedBytes[i] == '=' && i + 2 < encodedBytes.size()) {
                            QByteArray hex = encodedBytes.mid(i + 1, 2);
                            result.append(static_cast<char>(hex.toInt(nullptr, 16)));
                            i += 2;
                        } else {
                            result.append(encodedBytes[i]);
                        }
                    }
                    decodedBytes = result;
                }

                // Convert bytes to QString using the specified charset
                if (charset.toLower() == "utf-8") {
                    decoded += QString::fromUtf8(decodedBytes);
                } else if (charset.toLower() == "iso-8859-1") {
                    decoded += QString::fromLatin1(decodedBytes);
                } else {
                    qDebug() << "Unsupported charset:" << charset;
                    decoded += QString::fromUtf8(decodedBytes);  // Fallback to UTF-8
                }
            }
        }
    } else {
        decoded = encodedString.trimmed();  // If not MIME-encoded, return as-is
    }

    return decoded;
}

