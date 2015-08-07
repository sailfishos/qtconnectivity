/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include "qbluetoothtransferreply_bluez_p.h"
#include "qbluetoothaddress.h"

#include "bluez/obex_client_p.h"
#include "bluez/obex_transfer_p.h"
#include "bluez/obex_objectpush_p.h"
#include "qbluetoothtransferreply.h"
#include "obextransfer.h"

#include <QtCore/QLoggingCategory>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QDebug>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

static inline QString orgBluezObexClientString()
{
    return QStringLiteral("org.bluez.obex.client");
}

QBluetoothTransferReplyBluez::QBluetoothTransferReplyBluez(QIODevice *input, const QBluetoothTransferRequest &request,
                                                           QBluetoothTransferManager *parent)
    :   QBluetoothTransferReply(parent), objectTransfer(0), objectPush(0), client(0), tempfile(0), source(input),
    m_running(false), m_finished(false), m_size(0),
    m_error(QBluetoothTransferReply::NoError), m_errorStr()
{
    setRequest(request);
    setManager(parent);
    client = new OrgBluezObexClientInterface(orgBluezObexClientString(),
                                             QStringLiteral("/"),
                                             QDBusConnection::sessionBus());

    qsrand(QTime::currentTime().msec());

    qRegisterMetaType<QBluetoothTransferReply*>("QBluetoothTransferReply*");
    QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
    m_running = true;
}

/*!
    Destroys the QBluetoothTransferReply object.
*/
QBluetoothTransferReplyBluez::~QBluetoothTransferReplyBluez()
{
    delete objectTransfer;
    delete objectPush;
    delete client;
}

void QBluetoothTransferReplyBluez::fail(QBluetoothTransferReply::TransferError err,
            const QString msg)
{
    m_finished = true;
    m_running = false;
    m_errorStr = msg;
    m_error = err;
    QMetaObject::invokeMethod(this, "finished",
                              Qt::QueuedConnection,
                              Q_ARG(QBluetoothTransferReply*, this));
}

bool QBluetoothTransferReplyBluez::start()
{
    QString dest = request().address().toString();
    QVariantMap args;
    args.insert(QStringLiteral("Target"), QStringLiteral("OPP"));

    if (!objectPush) {
        qCDebug(QT_BT_BLUEZ) << "Creating an outgoing session";
        QDBusPendingReply<> sessionReply = client->CreateSession(dest, args);
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(sessionReply, this);
        QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                         this, SLOT(sessionAcquired(QDBusPendingCallWatcher*)));
    } else {
        startPush();
    }
    return true;
}

void QBluetoothTransferReplyBluez::sessionAcquired(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<QDBusObjectPath> sessionReply = *watcher;
    watcher->deleteLater();
    if (sessionReply.isError()) {
        QString msg = sessionReply.error().message();
        qCDebug(QT_BT_BLUEZ) << "Failed to create a session:" << msg;
        fail(QBluetoothTransferReply::UnknownError, msg);
        return;
    }

    qCDebug(QT_BT_BLUEZ) << "Creating a push object for path" << sessionReply.value().path();
    delete objectPush;
    objectPush = new OrgBluezObexObjectPushInterface
        (orgBluezObexClientString(),
         sessionReply.value().path(),
         QDBusConnection::sessionBus());

    startPush();
}

void QBluetoothTransferReplyBluez::startPush()
{
//    qDebug() << "Got a:" << source->metaObject()->className();
    QFile *file = qobject_cast<QFile *>(source);

    if(!file){
        tempfile = new QTemporaryFile(this );
        tempfile->open();
        qCDebug(QT_BT_BLUEZ) << "Not a QFile, making a copy" << tempfile->fileName();

        QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
        QObject::connect(watcher, SIGNAL(finished()), this, SLOT(copyDone()));

        QFuture<bool> results = QtConcurrent::run(QBluetoothTransferReplyBluez::copyToTempFile, tempfile, source);
        watcher->setFuture(results);
    }
    else {
        if (!file->exists()) {
            fail(QBluetoothTransferReply::FileNotFoundError,
                 QBluetoothTransferReply::tr("File does not exist"));
            return;
        }
        if (request().address().isNull()) {
            fail(QBluetoothTransferReply::HostNotFoundError,
                 QBluetoothTransferReply::tr("Invalid target address"));
            return;
        }
        m_size = file->size();
        startOPP(file->fileName());
    }
}

bool QBluetoothTransferReplyBluez::copyToTempFile(QIODevice *to, QIODevice *from)
{
    char *block = new char[4096];
    int size;

    while((size = from->read(block, 4096))) {
        if(size != to->write(block, size)){
            return false;
        }
    }

    delete[] block;
    return true;
}

void QBluetoothTransferReplyBluez::copyDone()
{
    m_size = tempfile->size();
    startOPP(tempfile->fileName());
    QObject::sender()->deleteLater();
}

void QBluetoothTransferReplyBluez::startOPP(QString filename)
{
    qCDebug(QT_BT_BLUEZ) << "Pushing file" << filename;

    QDBusPendingReply<> sendReply = objectPush->SendFile(filename);

    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(sendReply, this);
    QObject::connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                     this, SLOT(sendReturned(QDBusPendingCallWatcher*)));
}

void QBluetoothTransferReplyBluez::sendReturned(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<ObexTransfer> sendReply = *watcher;
    watcher->deleteLater();
    if (sendReply.isError()) {
        QString msg = sendReply.error().message();
        qCDebug(QT_BT_BLUEZ) << "Send failed:" << sendReply.error().name()
                             << "," << msg;
        fail(QBluetoothTransferReply::UnknownError, msg);
        return;
    }

    qCDebug(QT_BT_BLUEZ) << "Starting to monitor transfer" << sendReply.value().getPath().path();

    delete objectTransfer;
    objectTransfer = new OrgBluezObexTransferInterface
        (orgBluezObexClientString(),
         sendReply.value().getPath().path(),
         QDBusConnection::sessionBus());
    QObject::connect(objectTransfer, SIGNAL(Complete()),
                     this, SLOT(Complete()));
    QObject::connect(objectTransfer,
                     SIGNAL(Error(const QString &, const QString &)),
                     this,
                     SLOT(Error(const QString &, const QString &)));
    QObject::connect(objectTransfer,
                     SIGNAL(PropertyChanged(const QString &, const QDBusVariant &)),
                     this,
                     SLOT(Progress(const QString &, const QDBusVariant &)));
}

QBluetoothTransferReply::TransferError QBluetoothTransferReplyBluez::error() const
{
    return m_error;
}

QString QBluetoothTransferReplyBluez::errorString() const
{
    return m_errorStr;
}

void QBluetoothTransferReplyBluez::Complete()
{
    qCDebug(QT_BT_BLUEZ) << "Transfer complete";
    delete objectTransfer;
    objectTransfer = NULL;
    m_finished = true;
    m_running = false;
    emit finished(this);
}

void QBluetoothTransferReplyBluez::Error(const QString &in0, const QString &in1)
{
    qCDebug(QT_BT_BLUEZ) << "Transfer error" << in0 << "," << in1;
    delete objectTransfer;
    objectTransfer = NULL;
    fail(QBluetoothTransferReply::UnknownError, in1);
    emit finished(this);
}

void QBluetoothTransferReplyBluez::Progress(const QString &in0, const QDBusVariant &in1)
{
    qCDebug(QT_BT_BLUEZ) << "Property indication for" << in0;

    if (in0 == QLatin1String("Progress")) {
        qlonglong position = in1.variant().toLongLong();
        emit transferProgress(position, m_size);
    }
}

/*!
    Returns true if this reply has finished; otherwise returns false.
*/
bool QBluetoothTransferReplyBluez::isFinished() const
{
    return m_finished;
}

/*!
    Returns true if this reply is running; otherwise returns false.
*/
bool QBluetoothTransferReplyBluez::isRunning() const
{
    return m_running;
}

void QBluetoothTransferReplyBluez::abort()
{
    if (objectTransfer) {
        QDBusPendingReply<> reply =objectTransfer->Cancel();
        reply.waitForFinished();
        if (reply.isError())
            qCWarning(QT_BT_BLUEZ) << "Failed to abort transfer" << reply.error();
        delete objectTransfer;
        objectTransfer = 0;
    }
}

#include "moc_qbluetoothtransferreply_bluez_p.cpp"

QT_END_NAMESPACE
