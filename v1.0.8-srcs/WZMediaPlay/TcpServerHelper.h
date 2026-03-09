#pragma once
#include <QTcpServer>
#include "TcpSocket.h"

class MainWindow;
class TcpServerHelper:public QTcpServer
{
	Q_OBJECT
public:
    explicit TcpServerHelper(QObject *parent = nullptr);
    ~TcpServerHelper();
    void setMainWindow(MainWindow *mw);

protected:
    void incomingConnection(qintptr sockerDescriptor);
signals:
    void newSockDescriptor(qintptr _sock);
public:
    QList<TcpSocket*> mSocketList;
    MainWindow *mWin;
};
