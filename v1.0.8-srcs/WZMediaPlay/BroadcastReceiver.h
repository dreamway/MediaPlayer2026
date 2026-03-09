#pragma once
#include <WinSock2.h>
#include <iostream>
#include <stdio.h>
#include <QObject>

#pragma comment(lib, "ws2_32.lib")

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