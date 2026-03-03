#pragma once
//#include <vector>
#include <QList>
#include <QObject>

class VideoInfo
{
public:
    QString video_path;
    QString video_total_time;
};

class PlayList
{
public:
    QString list_name;
    QList<VideoInfo> video_list;
};

class PlayListData
{
public:
    int playlist_current_index = 0;
    int playlist_current_video = -1;
    int64_t playlist_video_play_time = 0;
    QList<PlayList> play_list;
};
