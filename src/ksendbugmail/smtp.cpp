/*
   Copyright (c) 2000 Bernd Johannes Wuebben <wuebben@math.cornell.edu>
   Copyright (c) 2000 Stephan Kulow <coolo@kde.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "smtp.h"
#include "../systeminformation_p.h"

#include <stdio.h>

#include <QDebug>
#include <QSslSocket>
#include <QHostInfo>

SMTP::SMTP(char *serverhost, unsigned short int port, int timeout)
{
    serverHost = QString::fromUtf8(serverhost);
    hostPort = port;
    timeOut = timeout * 1000;

    senderAddress = QStringLiteral("user@example.net");
    recipientAddress = QStringLiteral("user@example.net");
    messageSubject = QStringLiteral("(no subject)");
    messageBody = QStringLiteral("empty");
    messageHeader = QLatin1String("");

    connected = false;
    finished = false;

    sock = nullptr;
    state = Init;
    serverState = None;

    domainName = QHostInfo::localDomainName();
    if (domainName.isEmpty()) {
        domainName = QStringLiteral("somemachine.example.net");
    }

    // qCDebug(DEBUG_KXMLGUI) << "SMTP object created";

    connect(&connectTimer, SIGNAL(timeout()), this, SLOT(connectTimerTick()));
    connect(&timeOutTimer, SIGNAL(timeout()), this, SLOT(connectTimedOut()));
    connect(&interactTimer, SIGNAL(timeout()), this, SLOT(interactTimedOut()));

    // some sendmail will give 'duplicate helo' error, quick fix for now
    connect(this, SIGNAL(messageSent()), SLOT(closeConnection()));
}

SMTP::~SMTP()
{
    delete sock;
    sock = nullptr;
    connectTimer.stop();
    timeOutTimer.stop();
}

void SMTP::setServerHost(const QString &serverhost)
{
    serverHost = serverhost;
}

void SMTP::setPort(unsigned short int port)
{
    hostPort = port;
}

void SMTP::setTimeOut(int timeout)
{
    timeOut = timeout;
}

void SMTP::setSenderAddress(const QString &sender)
{
    senderAddress = sender;
    int index = senderAddress.indexOf(QLatin1Char('<'));
    if (index == -1) {
        return;
    }
    senderAddress = senderAddress.mid(index + 1);
    index =  senderAddress.indexOf(QLatin1Char('>'));
    if (index != -1) {
        senderAddress = senderAddress.left(index);
    }
    senderAddress = senderAddress.simplified();
    while (1) {
        index =  senderAddress.indexOf(QLatin1Char(' '));
        if (index != -1) {
            senderAddress = senderAddress.mid(index + 1);    // take one side
        } else {
            break;
        }
    }
    index = senderAddress.indexOf(QLatin1Char('@'));
    if (index == -1) {
        senderAddress.append(QStringLiteral("@localhost"));    // won't go through without a local mail system
    }

}

void SMTP::setRecipientAddress(const QString &recipient)
{
    recipientAddress = recipient;
}

void SMTP::setMessageSubject(const QString &subject)
{
    messageSubject = subject;
}

void SMTP::setMessageBody(const QString &message)
{
    messageBody = message;
}

void SMTP::setMessageHeader(const QString &header)
{
    messageHeader = header;
}

void SMTP::openConnection(void)
{
    // qCDebug(DEBUG_KXMLGUI) << "started connect timer";
    connectTimer.setSingleShot(true);
    connectTimer.start(100);
}

void SMTP::closeConnection(void)
{
    socketClosed();
}

void SMTP::sendMessage(void)
{
    if (!connected) {
        connectTimerTick();
    }
    if (state == Finished && connected) {
        // qCDebug(DEBUG_KXMLGUI) << "state was == Finished\n";
        finished = false;
        state = In;
        writeString = QStringLiteral("helo %1\r\n").arg(domainName);
        sock->write(writeString.toLatin1().constData(), writeString.length());
    }
    if (connected) {
        // qCDebug(DEBUG_KXMLGUI) << "enabling read on sock...\n";
        interactTimer.setSingleShot(true);
        interactTimer.start(timeOut);
    }
}

void SMTP::connectTimerTick(void)
{
    connectTimer.stop();
//    timeOutTimer.start(timeOut, true);

    // qCDebug(DEBUG_KXMLGUI) << "connectTimerTick called...";

    delete sock;
    sock = nullptr;

    // qCDebug(DEBUG_KXMLGUI) << "connecting to " << serverHost << ":" << hostPort << " ..... ";
    sock = new QSslSocket(this);
    sock->connectToHost(serverHost, hostPort);

    connected = true;
    finished = false;
    state = Init;
    serverState = None;

    connect(sock, SIGNAL(readyRead()), this, SLOT(socketReadyToRead()));
    connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this,
            SLOT(socketError(QAbstractSocket::SocketError)));
    connect(sock, SIGNAL(disconnected()), this, SLOT(socketClosed()));
    timeOutTimer.stop();
    // qCDebug(DEBUG_KXMLGUI) << "connected";
}

void SMTP::connectTimedOut(void)
{
    timeOutTimer.stop();

    // qCDebug(DEBUG_KXMLGUI) << "socket connection timed out";
    socketClosed();
    emit error(ConnectTimeout);
}

void SMTP::interactTimedOut(void)
{
    interactTimer.stop();

    // qCDebug(DEBUG_KXMLGUI) << "time out waiting for server interaction";
    socketClosed();
    emit error(InteractTimeout);
}

void SMTP::socketReadyToRead()
{
    int n, nl;

    // qCDebug(DEBUG_KXMLGUI) << "socketRead() called...";
    interactTimer.stop();

    if (!sock) {
        return;
    }

    n = sock->read(readBuffer, SMTP_READ_BUFFER_SIZE - 1);
    if (n < 0) {
        return;
    }
    readBuffer[n] = 0;
    lineBuffer += QByteArray(readBuffer);
    nl = lineBuffer.indexOf('\n');
    if (nl == -1) {
        return;
    }
    lastLine = lineBuffer.left(nl);
    lineBuffer = lineBuffer.right(lineBuffer.length() - nl - 1);
    processLine(&lastLine);
    if (connected) {
        interactTimer.setSingleShot(true);
        interactTimer.start(timeOut);
    }
}

void SMTP::socketError(QAbstractSocket::SocketError socketError)
{
    // qCDebug(DEBUG_KXMLGUI) << socketError << sock->errorString();
    Q_UNUSED(socketError);
    emit error(ConnectError);
    socketClosed();
}

void SMTP::socketClosed()
{
    timeOutTimer.stop();
    // qCDebug(DEBUG_KXMLGUI) << "connection terminated";
    connected = false;
    if (sock) {
        sock->deleteLater();
    }
    sock = nullptr;
    emit connectionClosed();
}

void SMTP::processLine(QByteArray *line)
{
    int i, stat;
    QByteArray tmpstr;

    i = line->indexOf(' ');
    tmpstr = line->left(i);
    if (i > 3) {
        // qCDebug(DEBUG_KXMLGUI) << "warning: SMTP status code longer than 3 digits: " << tmpstr;
    }
    stat = tmpstr.toInt();
    serverState = static_cast<SMTPServerStatus>(stat);
    lastState = state;

    // qCDebug(DEBUG_KXMLGUI) << "smtp state: [" << stat << "][" << *line << "]";

    switch (stat) {
    case Greet:     //220
        state = In;
        writeString = QStringLiteral("helo %1\r\n").arg(domainName);
        // qCDebug(DEBUG_KXMLGUI) << "out: " << writeString;
        sock->write(writeString.toLatin1().constData(), writeString.length());
        break;
    case Goodbye:   //221
        state = Quit;
        break;
    case Successful://250
        switch (state) {
        case In:
            state = Ready;
            writeString = QStringLiteral("mail from: %1\r\n").arg(senderAddress);
            // qCDebug(DEBUG_KXMLGUI) << "out: " << writeString;
            sock->write(writeString.toLatin1().constData(), writeString.length());
            break;
        case Ready:
            state = SentFrom;
            writeString = QStringLiteral("rcpt to: %1\r\n").arg(recipientAddress);
            // qCDebug(DEBUG_KXMLGUI) << "out: " << writeString;
            sock->write(writeString.toLatin1().constData(), writeString.length());
            break;
        case SentFrom:
            state = SentTo;
            writeString = QStringLiteral("data\r\n");
            // qCDebug(DEBUG_KXMLGUI) << "out: " << writeString;
            sock->write(writeString.toLatin1().constData(), writeString.length());
            break;
        case Data:
            state = Finished;
            finished = true;
            emit messageSent();
            break;
        default:
            state = CError;
            // qCDebug(DEBUG_KXMLGUI) << "smtp error (state error): [" << lastState << "]:[" << stat << "][" << *line << "]";
            socketClosed();
            emit error(Command);
            break;
        }
        break;
    case ReadyData: //354
        state = Data;
        writeString = QStringLiteral("Subject: %1\r\n").arg(messageSubject);
        writeString += messageHeader;
        writeString += QLatin1String("\r\n");
        writeString += messageBody;
        writeString += QLatin1String(".\r\n");
        // qCDebug(DEBUG_KXMLGUI) << "out: " << writeString;
        sock->write(writeString.toLatin1().constData(), writeString.length());
        break;
    case Error:     //501
        state = CError;
        // qCDebug(DEBUG_KXMLGUI) << "smtp error (command error): [" << lastState << "]:[" << stat << "][" << *line << "]\n";
        socketClosed();
        emit error(Command);
        break;
    case Unknown:   //550
        state = CError;
        // qCDebug(DEBUG_KXMLGUI) << "smtp error (unknown user): [" << lastState << "]:[" << stat << "][" << *line << "]";
        socketClosed();
        emit error(UnknownUser);
        break;
    default:
        state = CError;
        // qCDebug(DEBUG_KXMLGUI) << "unknown response: [" << lastState << "]:[" << stat << "][" << *line << "]";
        socketClosed();
        emit error(UnknownResponse);
    }
}

