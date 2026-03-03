#pragma once

#include "PlaylistItem.h"
#include <QObject>
#include <QList>
#include <QRandomGenerator>
#include <memory>

namespace playlist {

/**
 * Playlist: 播放列表管理类
 * 
 * 职责：
 * - 管理播放列表项的增删改查
 * - 提供播放控制（下一首、上一首、随机播放等）
 * - 跟踪当前播放项
 * - 支持多种播放模式
 * 
 * 设计：
 * - 继承 QObject，支持信号槽机制
 * - 使用 QList 存储播放项
 * - 播放模式影响 next()/previous() 的行为
 */
class Playlist : public QObject {
    Q_OBJECT

public:
    // 播放模式
    enum class PlayMode {
        Sequential,     // 顺序播放
        Loop,           // 列表循环
        Random,         // 随机播放
        SingleLoop      // 单曲循环
    };
    Q_ENUM(PlayMode)

    explicit Playlist(QObject* parent = nullptr);
    ~Playlist() override;

    // 播放列表管理
    void addItem(const PlaylistItem& item);
    void addItems(const QList<PlaylistItem>& items);
    void insertItem(int index, const PlaylistItem& item);
    void removeItem(int index);
    void removeItem(const PlaylistItem& item);
    void clear();

    // 查询
    int count() const { return items_.count(); }
    bool isEmpty() const { return items_.isEmpty(); }
    PlaylistItem itemAt(int index) const;
    int indexOf(const PlaylistItem& item) const;
    int indexOf(const QString& filename) const;
    QList<PlaylistItem> items() const { return items_; }

    // 当前播放项
    int currentIndex() const { return currentIndex_; }
    PlaylistItem currentItem() const;
    bool hasCurrentItem() const { return currentIndex_ >= 0 && currentIndex_ < items_.count(); }

    // 播放控制
    void setCurrentIndex(int index);
    bool playNext();        // 返回是否成功切换到下一首
    bool playPrevious();    // 返回是否成功切换到上一首
    bool playFirst();       // 播放第一首
    bool playLast();        // 播放最后一首

    // 播放模式
    PlayMode playMode() const { return playMode_; }
    void setPlayMode(PlayMode mode);

    // 随机播放历史（用于随机模式下返回上一首）
    bool canGoBackInRandomMode() const { return !randomHistory_.isEmpty(); }

    // 保存/加载播放列表
    bool saveToFile(const QString& filename) const;
    bool loadFromFile(const QString& filename);

    // 查找（按文件名或标题）
    QList<int> findByFilename(const QString& pattern) const;
    QList<int> findByTitle(const QString& pattern) const;

signals:
    void currentItemChanged(int index, const PlaylistItem& item);
    void itemAdded(int index, const PlaylistItem& item);
    void itemRemoved(int index);
    void itemUpdated(int index, const PlaylistItem& item);
    void playModeChanged(PlayMode mode);
    void playlistCleared();

private:
    // 获取下一首索引（根据播放模式）
    int getNextIndex() const;
    int getPreviousIndex() const;
    int getRandomIndex() const;

    QList<PlaylistItem> items_;
    int currentIndex_ = -1;
    PlayMode playMode_ = PlayMode::Sequential;

    // 随机播放历史
    QList<int> randomHistory_;
    int maxRandomHistorySize_ = 50;

    // 随机数生成器
    QRandomGenerator randomGenerator_;
};

} // namespace playlist