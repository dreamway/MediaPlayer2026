#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
class PlayController;

class TestPipeServer : public QObject
{
    Q_OBJECT
public:
    explicit TestPipeServer(PlayController *controller, QObject *parent = nullptr);
    ~TestPipeServer();
    bool start();
    void stop();

private slots:
    void onTimer();

private:
    PlayController *controller_;
    QTimer *timer_;
    void *pipeHandle_;  // HANDLE on Windows
    bool writeStatus(const QString &json);
};
