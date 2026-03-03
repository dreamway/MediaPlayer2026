#include "RemoteControlServer.h"
#include<QThread>
#include <QDebug>

RemoteControlServer::RemoteControlServer(MainWindow *mw)
{
    //server = new QTcpServer(this);
    //server->listen(QHostAddress::Any, TCPSERVER_PORT);
    //connect(server, SIGNAL(newConnection()), this, SLOT(remoteControlConnect()));

    mTcpServerHelper = new TcpServerHelper();
    mTcpServerHelper->setMainWindow(mw);
    mTcpServerHelper->listen(QHostAddress::Any, TCPSERVER_PORT);

}

RemoteControlServer ::~RemoteControlServer() {
}

void RemoteControlServer::remoteControlConnect() {
    qDebug() << "================================== " << __FUNCTION__ << "  thread id:" << QThread::currentThreadId();
    //QTcpSocket *socket = server->nextPendingConnection();
    //connect(socket, &QTcpSocket::readyRead, [this, socket] {
    //    QByteArray bytes = socket->readAll();
    //    QByteArray returnBytes;
    //    if (bytes.size() >= 9) {
    //        if (bytes[0] != (char) 0xA3 || bytes[bytes.size() - 1] != (char) 0x3A) {
    //            qDebug() << "====================================receive head or end false...";
    //            returnBytes = QByteArray::fromHex("A3 00 01 00 00 00 00 01 00 3A");
    //        } else {
    //            qDebug() << "=====================================  read bytes size:" << bytes.size();
    //            int len = -1;
    //            memcpy(&len, &bytes[2], 4);
    //            qDebug() << "---  net work data len:" << len;
    //            if ((len + 9) == bytes.size()) {
    //                unsigned short verSum = bytes[1] + len;
    //                for (int i = 0; i < len; ++i) {
    //                    verSum = verSum + bytes[6 + i];
    //                }
    //                unsigned short verSend = ((bytes[6 + len] << 8) & 0xFF00) | (bytes[7 + len] & 0xFF);
    //                //std::cout << "verSum:" << std::hex << (unsigned int)verSum << std::endl;
    //                //std::cout<< "verSend:" <<std::hex<< (unsigned int)verSend << std::endl;
    //                if (verSum == verSend) {
    //                    returnBytes = QByteArray::fromHex("A3");
    //                    returnBytes.append(bytes[1]);
    //                    returnBytes.append((uchar) 0x01);
    //                    returnBytes.append((uchar) 0x00);
    //                    returnBytes.append((uchar) 0x00);
    //                    returnBytes.append((uchar) 0x00);
    //                    returnBytes.append((uchar) 0x01);
    //                    uint16_t sumA = bytes[1] + 2;
    //                    QByteArray temp;
    //                    temp.resize(2);
    //                    memcpy(&temp, &sumA, 2);
    //                    returnBytes.append(temp[0]);
    //                    returnBytes.append(temp[1]);
    //                    returnBytes.append((uchar) 0x3A);
    //                    qDebug() << "------------------------------  net work send cmd:";
    //                    qDebug() << "------------------  return bytes size:" << returnBytes.size() << " data:" << returnBytes.toHex();
    //                    socket->write(returnBytes);
    //                    emit sendCMD(bytes[1]);
    //                    return;
    //                    qDebug()<<"-------   RemoteControlServer after sendCMD";
    //                } else {
    //                    returnBytes = QByteArray::fromHex("A3 00 01 00 00 00 00 01 00 3A");
    //                }
    //            } else {
    //                returnBytes = QByteArray::fromHex("A3 00 01 00 00 00 00 01 00 3A");
    //            }
    //        }
    //    } else {
    //        returnBytes = QByteArray::fromHex("A3 00 01 00 00 00 00 01 00 3A");
    //    }

    //    qDebug() << "returnBytes :" << returnBytes.toHex();
    //    socket->write(returnBytes);
    //    qDebug() << "--------- socket write returnBytes...";
    //    //socket->close();
    //    });
}