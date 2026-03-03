#pragma once

#include "PlayListData.h"
#include "Playlist.h"
#include <QObject>
#include <QHash>
#include <memory>

/**
 * PlayListManager: 播放列表管理器
 * 
 * 职责：
 * - 管理多个播放列表（支持多播放列表切换）
 * - 提供播放控制（下一首、上一首、随机播放等）
 * - 跟踪当前播放项
 * - 支持多种播放模式
 * 
 * 设计：
 * - 内部使用 Playlist 类管理每个播放列表
 * - 保留与 GlobalDef 的同步（向后兼容）
 * - 添加播放模式支持
 */
class PlayListManager : public QObject {
    Q_OBJECT

public:
    // 播放模式（与 Playlist::PlayMode 对应）
    enum class PlayMode {
        Sequential,     // 顺序播放
        Loop,           // 列表循环
        Random,         // 随机播放
        SingleLoop      // 单曲循环
    };
    Q_ENUM(PlayMode)

    explicit PlayListManager(QObject* parent = nullptr);
    ~PlayListManager() override;

    // 播放列表管理
    bool addVideoToPlayList(const QString& videoPath, int playListIndex = 0);
    bool removeVideoFromPlayList(int videoIndex, int playListIndex = 0);
    bool clearPlayList(int playListIndex = 0);

    // 查询
    int getPlayListSize(int playListIndex = 0) const;
    QString getVideoPath(int videoIndex, int playListIndex = 0) const;
    int getCurrentVideoIndex(int playListIndex = 0) const;
    
    // 当前播放项
    void setCurrentVideo(int videoIndex, int playListIndex = 0, bool emitSignal = true);
    QString getCurrentVideoPath() const;
    
    // 播放控制（支持播放模式）
    bool hasNextVideo() const;
    bool hasPreviousVideo() const;
    QString getNextVideoPath() const;
    QString getPreviousVideoPath() const;
    bool switchToNextVideo();
    bool switchToPreviousVideo();
    bool switchToVideo(int index, int playListIndex = 0);

    // 播放模式
    PlayMode playMode() const { return playMode_; }
    void setPlayMode(PlayMode mode);

    // 获取内部 Playlist 对象（用于高级操作）
    playlist::Playlist* getPlaylist(int playListIndex = 0) const;

    // 从 PlayListData 加载（向后兼容）
    void loadFromPlayListData(const PlayListData& data);
    void saveToPlayListData(PlayListData& data) const;

signals:
    void playListChanged(int playListIndex);
    void currentVideoChanged(const QString& videoPath);
    void playModeChanged(PlayMode mode);

private:
    bool isValidIndex(int videoIndex, int playListIndex) const;
    void updateCurrentVideoIndex(int videoIndex, int playListIndex);
    void syncToGlobalDef();

    // 内部播放列表映射（使用裸指针，由PlayListManager管理生命周期）
    QHash<int, playlist::Playlist*> playlists_;
    
    // 当前播放信息
    int currentPlayListIndex_ = 0;
    int currentVideoIndex_ = -1;
    
    // 播放模式
    PlayMode playMode_ = PlayMode::Sequential;
    
    // 向后兼容：保留 PlayListData
    PlayListData playListData_;
};
