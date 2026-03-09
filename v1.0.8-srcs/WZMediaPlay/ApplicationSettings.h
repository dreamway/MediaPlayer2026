#pragma once
#include <QObject>
#include <QSettings>

class PlayList;
/**
程序的全局配置信息
*/
class ApplicationSettings : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationSettings(QObject *parent = 0);
    ~ApplicationSettings();
    void read_ALL();
    void write_ALL();

    QString ToString();

    void read_SplashLogoPath();
    void write_SplashLogoPath(QString path);

    void read_ApplicationGeneral();
    void write_ApplicationGeneral();

    void read_Hotkey();
    void write_Hotkey();

    void read_WindowSizeState();
    void write_WindowSizeState();

    void read_PlayState();
    void write_PlayState();

    void read_PlayList();
    void write_PlayList();
    void clear_PlayList();
    PlayList load_PlayList(QString path);
    void export_PlayList(QString path, int index);

    void read_about_copyright();

private:
    qint64 qJsonValue2longlong(QJsonValue value);
};
