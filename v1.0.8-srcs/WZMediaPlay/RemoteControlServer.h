#pragma once
#include <QObject>
//#include <QTcpServer>
//#include <QTCPSocket>
#include "TcpServerHelper.h"
#define TCPSERVER_PORT 9529

class MainWindow;

class RemoteControlServer:public QObject
{
    Q_OBJECT
public:
    RemoteControlServer(MainWindow *mw);
    ~RemoteControlServer();
signals:
    void sendCMD(char cmd);
private slots:
    void remoteControlConnect();

private:
    //QTcpServer *server;
    TcpServerHelper *mTcpServerHelper;
};