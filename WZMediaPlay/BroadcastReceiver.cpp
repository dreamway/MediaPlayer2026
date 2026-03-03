#include "BroadcastReceiver.h"
#include <QThread>
#include "MainWindow.h"
#include <future>
#include <thread>

#define INT_MAX_PACKET_SIZE 1024
#define MYPORT 9527

BroadcastReceiver::BroadcastReceiver(QObject *parent)
    : QObject(parent)
{}

BroadcastReceiver::~BroadcastReceiver() {}

void BroadcastReceiver::onBroadcastReceive()
{
#ifdef Q_OS_WIN
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        std::cerr << "Error in WSAStartup.code:" << err << std::endl;
        return;
    }

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        WSACleanup();
        std::cerr << "Error in LOBYTE/HIBYTE" << std::endl;
        return;
    }

    SOCKET connect_socket;
    connect_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (INVALID_SOCKET == connect_socket) {
        err = WSAGetLastError();
        std::cerr << "Error in socket creation.code:" << err << std::endl;
        return;
    }

    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(MYPORT);
    sin.sin_addr.s_addr = 0;

    SOCKADDR_IN sin_from;
    sin_from.sin_family = AF_INET;
    sin_from.sin_port = htons(MYPORT);
    sin_from.sin_addr.s_addr = INADDR_BROADCAST;

    bool bOpt = true;
    setsockopt(connect_socket, SOL_SOCKET, SO_BROADCAST, (char *) &bOpt, sizeof(bOpt));

    err = ::bind(connect_socket, (SOCKADDR *) &sin, sizeof(SOCKADDR));
    if (SOCKET_ERROR == err) {
        err = WSAGetLastError();
        std::cerr << "Error in bind,code:" << err << std::endl;
        return;
    }

    int nAddrLen = sizeof(SOCKADDR);
    char buff[MAX_BUF_LEN] = "";

    while (1) {
        int nSendSize = recvfrom(connect_socket, buff, MAX_BUF_LEN, 0, (SOCKADDR *) &sin_from, &nAddrLen);
        if (SOCKET_ERROR == nSendSize) {
            err = WSAGetLastError();
            std::cerr << "Error in recvfrom,code:" << err << std::endl;
            return;
        }
#else
    int connect_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (connect_socket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(MYPORT);
    sin.sin_addr.s_addr = INADDR_ANY;

    int optval = 1;
    setsockopt(connect_socket, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
    setsockopt(connect_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (::bind(connect_socket, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        std::cerr << "Error in bind" << std::endl;
        close(connect_socket);
        return;
    }

    struct sockaddr_in sin_from;
    socklen_t nAddrLen = sizeof(sin_from);
    char buff[MAX_BUF_LEN] = "";

    while (1) {
        ssize_t nSendSize = recvfrom(connect_socket, buff, MAX_BUF_LEN, 0, (struct sockaddr *) &sin_from, &nAddrLen);
        if (nSendSize < 0) {
            std::cerr << "Error in recvfrom" << std::endl;
            close(connect_socket);
            return;
        }
#endif
        buff[nSendSize] = '\0';

        logger->debug("receive data len:{}", nSendSize);

        bool b_ver = false;
        char cmd = 0x00;
        if (nSendSize >= 9) {
            if (buff[0] != (char) 0xA3 || buff[nSendSize - 1] != (char) 0x3A) {
                b_ver = false;
                logger->warn("receive head or end false...");
            } else {
                int len = -1;
                memcpy(&len, &buff[2], 4);

                logger->info("receive data len: {}", len);
                logger->info("cmd: {:x}", (unsigned int) (unsigned char) buff[1]);
                if ((len + 9) == nSendSize) {
                    unsigned short verSum = buff[1] + len;
                    for (int i = 0; i < len; ++i) {
                        verSum = verSum + buff[6 + i];
                    }
                    unsigned short verSend = ((buff[6 + len] << 8) & 0xFF00) | (buff[7 + len] & 0xFF);
                    if (verSum == verSend) {
                        b_ver = true;
                        cmd = buff[1];
                    }
                }
            }
        }

        if (b_ver) {
            logger->info("receive cmd success.cmd: {:x}", (unsigned int) (unsigned char) cmd);
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
