#pragma once
#ifdef Q_OS_WIN
#include <WinSock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <iostream>
#include <stdio.h>
#include <QObject>

const int MAX_BUF_LEN = 255;

/**
手机控制
*/
class BroadcastReceiver : public QObject
{
    Q_OBJECT
public:
    explicit BroadcastReceiver(QObject *parent = nullptr);
    ~BroadcastReceiver();
    void startBroadcastListen();
signals:
    void sendCMD(char cmd);

private:
    void onBroadcastReceive();
};