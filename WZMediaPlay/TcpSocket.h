#pragma once
#include <QTcpSocket>
#include <QHostAddress>

class TcpSocket:public QTcpSocket
{
	Q_OBJECT
public:
    TcpSocket(int socketdes, QTcpSocket *parent = NULL);
    ~TcpSocket();
signals:
    void sendCMD(char cmd);
public slots:
    void ReadAndParseData();
    void SocketErr(QAbstractSocket::SocketError socketErr);

private:

};
