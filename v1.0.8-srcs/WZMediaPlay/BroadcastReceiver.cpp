#include "BroadcastReceiver.h"
#include "MainWindow.h"
#include <future>
#include <thread>

#define INT_MAX_PACKET_SIZE 1024
#define MYPORT 9527 // the port users will be connecting to

BroadcastReceiver::BroadcastReceiver(QObject *parent)
    : QObject(parent)
{}

BroadcastReceiver::~BroadcastReceiver() {}

void BroadcastReceiver::onBroadcastReceive()
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    // 启动socket api
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        std::cerr << "Error in WSAStartup.code:" << err << std::endl;
        return;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        WSACleanup();
        std::cerr << "Error in LOBYTE/HIBYTE,cause by LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2" << std::endl;
        return;
    }

    // 创建socket
    SOCKET connect_socket;
    connect_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (INVALID_SOCKET == connect_socket) {
        err = WSAGetLastError();
        std::cerr << "Error in WSAGetLastError.code:" << err << std::endl;
        return;
    }

    // 用来绑定套接字
    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(MYPORT);
    sin.sin_addr.s_addr = 0;

    // 用来从网络上的广播地址接收数据
    SOCKADDR_IN sin_from;
    sin_from.sin_family = AF_INET;
    sin_from.sin_port = htons(MYPORT);
    sin_from.sin_addr.s_addr = INADDR_BROADCAST;

    //设置该套接字为广播类型，
    bool bOpt = true;
    setsockopt(connect_socket, SOL_SOCKET, SO_BROADCAST, (char *) &bOpt, sizeof(bOpt));

    // 绑定套接字
    err = ::bind(connect_socket, (SOCKADDR *) &sin, sizeof(SOCKADDR));
    if (SOCKET_ERROR == err) {
        err = WSAGetLastError();
        std::cerr << "Error in WSAGetLastError,code:" << err << std::endl;
        return;
    }

    int nAddrLen = sizeof(SOCKADDR);
    char buff[MAX_BUF_LEN] = "";
    int nLoop = 0;
    while (1) {
        // 接收数据
        int nSendSize = recvfrom(connect_socket, buff, MAX_BUF_LEN, 0, (SOCKADDR *) &sin_from, &nAddrLen);
        if (SOCKET_ERROR == nSendSize) {
            err = WSAGetLastError();
            std::cerr << "Error in WSAGetLastError,code:" << err << std::endl;
            return;
        }
        buff[nSendSize] = '/0';

        
        //    debug print receive data
        logger->debug("receive data len:%d  Receive data: ",nSendSize);
        for (int i = 0; i < nSendSize; ++i) {
            logger->debug("%x ", (unsigned int) (unsigned char) (buff[i]));
        }
        logger->debug("\n");

        bool b_ver = false;
        char cmd = 0x00;
        if (nSendSize >= 9) {
            if (buff[0] != (char) 0xA3 || buff[nSendSize - 1] != (char) 0x3A) {
                b_ver = false;
                logger->warn("receive head or end false...");
            } else {
                int len = -1;
                memcpy(&len, &buff[2], 4);

                logger->info( "receive data len: %d",len);
                logger->info("cmd:");
                logger->info("%x",(unsigned int) (unsigned char) buff[1] );
                if ((len + 9) == nSendSize) {
                    unsigned short verSum = buff[1] + len;
                    for (int i = 0; i < len; ++i) {
                        verSum = verSum + buff[6 + i];
                    }
                    unsigned short verSend = ((buff[6 + len] << 8) & 0xFF00) | (buff[7 + len] & 0xFF);
                    //std::cout << "verSum:" << std::hex << (unsigned int)verSum << std::endl;
                    //std::cout<< "verSend:" <<std::hex<< (unsigned int)verSend << std::endl;
                    if (verSum == verSend) {
                        b_ver = true;
                        cmd = buff[1];
                    }
                }
            }
        }

        if (b_ver) {
            logger->info("receive cmd success.cmd: %x", (unsigned int) (unsigned char) cmd );
            qDebug() << "0000000000000000000000000000000000000000000000000000000000000000000000000000000000 thread id:" << QThread::currentThreadId();
            emit sendCMD(cmd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void BroadcastReceiver::startBroadcastListen()
{
    std::thread threadListen(&BroadcastReceiver::onBroadcastReceive, this);
    threadListen.detach();
}