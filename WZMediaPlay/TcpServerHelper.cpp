#include "TcpServerHelper.h"
#include <QThread>
#include "MainWindow.h"

TcpServerHelper::TcpServerHelper(QObject *parent)
    : QTcpServer(parent)
{
}

void TcpServerHelper::setMainWindow(MainWindow *mw)
{
    mWin = mw;
}

TcpServerHelper ::~TcpServerHelper() {
    QList<TcpSocket *>::iterator it = mSocketList.begin();
    for (; it != mSocketList.end();) {
        TcpSocket *sock = *it;
        it = mSocketList.erase(it);
        sock->deleteLater();
        sock = NULL;
    }
    mSocketList.clear();
    this->close();  
}

void TcpServerHelper::incomingConnection(qintptr socketDescriptor) {
    //emit newSockDescriptor(socketDescriptor);
    qDebug() << "incomingConnection...";
    TcpSocket *socket = new TcpSocket(socketDescriptor);

    connect(socket, &TcpSocket::sendCMD, mWin, &MainWindow::broadcastCallback);

    mSocketList.append(socket);

    connect(socket, SIGNAL(readyRead()), socket, SLOT(ReadAndParseData()));

    QThread *thread = new QThread();
    connect(socket, SIGNAL(disconnected()), thread, SLOT(quit()));
    connect(thread, SIGNAL(finished()), socket, SLOT(deleteLater()));

    socket->moveToThread(thread);
    thread->start();

    emit newConnection();

}