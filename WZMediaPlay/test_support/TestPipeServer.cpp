#include "TestPipeServer.h"
#include "PlayController.h"
#include "GlobalDef.h"
#include "videoDecoder/AVClock.h"
#include "videoDecoder/chronons.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#ifdef _WIN32
#include <windows.h>
#endif

TestPipeServer::TestPipeServer(PlayController *controller, QObject *parent)
    : QObject(parent), controller_(controller), pipeHandle_(nullptr)
{
    timer_ = new QTimer(this);
    connect(timer_, &QTimer::timeout, this, &TestPipeServer::onTimer);
}

TestPipeServer::~TestPipeServer()
{
    stop();
}

bool TestPipeServer::start()
{
#ifdef _WIN32
    const char *pipeName = "\\\\.\\pipe\\WZMediaPlayer_Test";
    pipeHandle_ = CreateNamedPipeA(
        pipeName,
        PIPE_ACCESS_OUTBOUND,
        PIPE_TYPE_BYTE,
        1, 512, 512, 0, nullptr);
    if (pipeHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    timer_->start(300);  // 300ms
    return true;
#else
    return false;
#endif
}

void TestPipeServer::stop()
{
    timer_->stop();
#ifdef _WIN32
    if (pipeHandle_ && pipeHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(static_cast<HANDLE>(pipeHandle_));
        pipeHandle_ = nullptr;
    }
#endif
}

void TestPipeServer::onTimer()
{
#ifdef _WIN32
    if (!pipeHandle_ || pipeHandle_ == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!ConnectNamedPipe(static_cast<HANDLE>(pipeHandle_), nullptr)
        && GetLastError() != ERROR_PIPE_CONNECTED) {
        return;
    }
    QJsonObject obj;
    obj["ts"] = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    obj["vol"] = controller_->getVolume();
    auto *clock = controller_->getAVClock();
    obj["audio_pts"] = clock ? clock->pts() : 0;
    obj["video_pts"] = clock ? clock->videoTime() : 0;
    obj["playing"] = controller_->isPlaying();
    obj["muted"] = GlobalDef::getInstance()->B_VOLUME_MUTE;
    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact) + "\n";
    DWORD written;
    WriteFile(static_cast<HANDLE>(pipeHandle_), json.constData(),
              static_cast<DWORD>(json.size()), &written, nullptr);
#endif
}
