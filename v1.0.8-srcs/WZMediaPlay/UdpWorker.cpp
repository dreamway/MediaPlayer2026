#include "UdpWorker.h"
#include <QHostInfo>
#include <QThread>
#include <QDebug>

UdpWorker::UdpWorker()
{
    socket = new QUdpSocket();
    //socket->bind(QHostAddress::Any, UDP_SEND_PORT);
}

UdpWorker ::~UdpWorker(){
}

void UdpWorker::sendIPAddress()
{
    QHostInfo info = QHostInfo::fromName(QHostInfo::localHostName());

    qDebug() << "IP Address:" << info.addresses();

    foreach (QHostAddress address, info.addresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            qDebug() << "localHost IPv4 address:" << address.toString();
            for (int i = 0; i < 5; ++i) {
                socket->writeDatagram(address.toString().toUtf8(), QHostAddress::Broadcast, UDP_SEND_PORT);
                QThread::msleep(20);
            }
        }
    }
}