#pragma once
#include <QObject>
#include <QUdpSocket>

#define UDP_SEND_PORT 9528

class UdpWorker : public QObject
{
    Q_OBJECT
public:
    UdpWorker();
    ~UdpWorker();
    void sendIPAddress();

private:
    QUdpSocket *socket = nullptr;
};
