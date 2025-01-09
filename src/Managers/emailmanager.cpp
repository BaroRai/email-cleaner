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

void EmailManager::applyCleanupRules(const QString &repository, const QStringList &selectedSenders, bool deleteRead, bool excludeAttachments) {
    if (!m_conn || !m_conn->isConnected()) {
        emit progressUpdated(0, "Not connected to the server.");
        emit cleanupCompleted();
        return;
    }

    QString encodedRepo = m_conn->encodeModifiedUTF7(repository);
    m_conn->sendCommand(QString("A010 SELECT \"%1\"\r\n").arg(encodedRepo));
    QString selectResponse = m_conn->readResponse();

    if (!selectResponse.contains("A010 OK")) {
        emit progressUpdated(0, "Failed to select repository.");
        emit cleanupCompleted();
        return;
    }

    QString searchCmd = deleteRead ? "A011 SEARCH ALL\r\n" : "A011 SEARCH UNSEEN\r\n";
    m_conn->sendCommand(searchCmd);
    QString searchResp = m_conn->readResponse();

    QStringList msgIDs = extractMessageIDsFromResponse(searchResp);
    if (msgIDs.isEmpty()) {
        emit showAlert("No Emails Found", "No emails matched the criteria in the selected repository.");
        emit cleanupCompleted();
        return;
    }

    int totalMessages = msgIDs.size();
    int processed = 0;

    for (const QString &msgID : msgIDs) {
        QString sender = fetchSenderForMessage(msgID);
        if (!selectedSenders.contains(sender, Qt::CaseInsensitive)) {
            ++processed;
            emit progressUpdated((processed * 100) / totalMessages, "Skipping emails...");
            continue;
        }

        QStringList reasons;

        if (!deleteRead && isEmailRead(msgID)) {
            reasons.append("Email is read, but 'Delete Read Emails' is not checked.");
        }

        if (excludeAttachments && emailHasAttachments(msgID)) {
            reasons.append("Email has attachments, but 'Delete Emails with Attachments' is not checked.");
        }

        if (!reasons.isEmpty()) {
            ++processed;
            emit progressUpdated((processed * 100) / totalMessages, "Skipping emails...");
            emit showAlert(
                "Email Skipped",
                QString("Email ID: %1\nReasons:\n- %2").arg(msgID, reasons.join("\n- "))
                );
            continue;
        }

        m_conn->sendCommand(QString("A013 STORE %1 +FLAGS (\\Deleted)\r\n").arg(msgID));
        QString storeResp = m_conn->readResponse();

        if (!storeResp.contains("A013 OK")) {
            ++processed;
            emit progressUpdated((processed * 100) / totalMessages, "Skipping emails...");
            emit showAlert("Error", QString("Failed to delete email ID: %1").arg(msgID));
            continue;
        }

        // Successfully deleted the email
        emit senderDeleted(sender);  // Notify MainWindow to remove sender from the table
        ++processed;
        emit progressUpdated((processed * 100) / totalMessages, "Deleting emails...");
        qDebug() << "Successfully deleted email ID:" << msgID;
    }

    emit cleanupCompleted();
    emit progressUpdated(100, "Cleanup completed successfully.");
    qDebug() << "Cleanup completed.";
}

bool EmailManager::isEmailRead(const QString &msgID) {
    QString fetchCmd = QString("A014 FETCH %1 (FLAGS)\r\n").arg(msgID);
    m_conn->sendCommand(fetchCmd);
    QString fetchResp = m_conn->readResponse();

    // Check if the FLAGS response includes the \Seen flag (indicating the email is read)
    return fetchResp.contains("\\Seen");
}


QStringList EmailManager::findSendersByMessageID(const QString &msgID, const QStringList &selectedSenders)
{
    QStringList matchingSenders;

    // Fetch sender for the given message ID
    QString sender = fetchSenderForMessage(msgID);
    if (sender.isEmpty()) {
        qDebug() << "No sender found for message ID:" << msgID;
        return matchingSenders;
    }

    // Check if the fetched sender is in the list of selected senders
    if (selectedSenders.contains(sender, Qt::CaseInsensitive)) {
        matchingSenders.append(sender);
        qDebug() << "Matched sender:" << sender << "for message ID:" << msgID;
    } else {
        qDebug() << "Sender" << sender << "not in selected senders for message ID:" << msgID;
    }

    return matchingSenders;
}

QStringList EmailManager::extractMessageIDsFromResponse(const QString &response)
{
    QStringList lines = response.split("\r\n", Qt::SkipEmptyParts);
    QString messageIDsLine;

    for (const QString &ln : lines) {
        if (ln.startsWith("* SEARCH ")) {
            messageIDsLine = ln.mid(8).trimmed();
            break;
        }
    }

    return messageIDsLine.split(' ', Qt::SkipEmptyParts);
}

QString EmailManager::fetchSenderForMessage(const QString &msgID)
{
    QString fetchCmd = QString("A012 FETCH %1 (BODY[HEADER.FIELDS (FROM)])\r\n").arg(msgID);
    m_conn->sendCommand(fetchCmd);
    QString fetchResp = m_conn->readResponse();

    return extractSenderFromFetchResponse(fetchResp);
}

bool EmailManager::emailHasAttachments(const QString &msgID)
{
    QString fetchCmd = QString("A012 FETCH %1 (BODY[HEADER])\r\n").arg(msgID);
    m_conn->sendCommand(fetchCmd);
    QString fetchResp = m_conn->readResponse();

    return fetchResp.contains("Content-Disposition: attachment") ||
           fetchResp.contains("Content-Type: multipart/mixed");
}

void EmailManager::requestFetchSenders(const QString &repository)
{
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

