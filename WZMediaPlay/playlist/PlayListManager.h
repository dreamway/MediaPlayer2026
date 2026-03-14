#pragma once

#include "PlayListData.h"
#include "Playlist.h"
#include <QObject>
#include <QMap>
#include <memory>

/**
 * PlayListManager: 播放列表管理器
 *
 * 职责：
 * - 管理多个播放列表（支持多播放列表切换）
 * - 提供播放控制（下一首、上一首、随机播放等）
 * - 跟踪当前播放项
 * - 支持多种播放模式
 * - 单一数据源：直接从 JSON 文件加载/保存
 *
 * 设计：
 * - 内部使用 Playlist 类管理每个播放列表
 * - 使用 QMap 保证播放列表顺序（修复 QHash 顺序不稳定问题）
 * - 添加 isDirty 标志跟踪变化
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

    // ========== 新增：单一数据源相关方法 ==========

    /**
     * 从 JSON 文件加载播放列表
     * @param jsonFilePath JSON 文件路径，默认使用 ApplicationSettings 配置路径
     * @param validateFiles 是否验证文件存在性
     * @return 成功返回 true
     */
    bool loadFromJson(const QString& jsonFilePath = QString(), bool validateFiles = true);

    /**
     * 保存播放列表到 JSON 文件
     * @param jsonFilePath JSON 文件路径，默认使用 ApplicationSettings 配置路径
     * @return 成功返回 true
     */
    bool saveToJson(const QString& jsonFilePath = QString());

    /**
     * 检查播放列表是否有未保存的变化
     */
    bool isDirty() const { return isDirty_; }

    /**
     * 重置 dirty 标志（保存成功后调用）
     */
    void markClean() { isDirty_ = false; }

    /**
     * 获取不存在的文件列表（用于 UI 提示）
     */
    QStringList getMissingFiles(int playListIndex = 0) const;

    /**
     * 获取播放列表总数
     */
    int getPlaylistCount() const { return playlists_.size(); }

    /**
     * 获取所有播放列表索引（有序）
     */
    QList<int> getPlaylistIndices() const;

    /**
     * 移除所有不存在的文件
     * @return 移除的文件数量
     */
    int removeMissingFiles();

    /**
     * 获取所有播放列表中不存在的文件总数
     */
    int getTotalMissingFilesCount() const;

signals:
    void playListChanged(int playListIndex);
    void currentVideoChanged(const QString& videoPath);
    void playModeChanged(PlayMode mode);

    /**
     * 文件验证完成信号，携带不存在的文件列表
     * @param missingFiles 不存在的文件路径列表
     */
    void fileValidationComplete(const QStringList& missingFiles);

private:
    bool isValidIndex(int videoIndex, int playListIndex) const;
    void updateCurrentVideoIndex(int videoIndex, int playListIndex);
    void syncToGlobalDef();

    /**
     * 标记数据已变化
     */
    void markDirty() { isDirty_ = true; }

    /**
     * 验证播放列表中的文件存在性
     * @return 不存在的文件列表
     */
    QStringList validateFileExistence();

    // 内部播放列表映射（使用 QMap 保证顺序，修复 QHash 顺序不稳定问题）
    QMap<int, playlist::Playlist*> playlists_;

    // 当前播放信息
    int currentPlayListIndex_ = 0;
    int currentVideoIndex_ = -1;

    // 播放模式
    PlayMode playMode_ = PlayMode::Sequential;

    // 向后兼容：保留 PlayListData
    PlayListData playListData_;

    // ========== 新增成员 ==========

    // 数据变化标志
    bool isDirty_ = false;

    // JSON 文件路径（缓存）
    QString jsonFilePath_;
};
