#pragma once

#include <QString>
#include <QMetaType>

namespace playlist {

/**
 * PlaylistItem: 播放列表项
 * 
 * 职责：
 * - 存储单个播放项的信息（文件名、标题、时长等）
 * - 提供元数据访问接口
 * 
 * 设计：
 * - 轻量级数据类，可复制
 * - 支持 Qt 元对象系统（用于信号槽传递）
 */
class PlaylistItem {
public:
    PlaylistItem() = default;
    explicit PlaylistItem(const QString& filename);
    PlaylistItem(const QString& filename, const QString& title, int64_t duration);
    ~PlaylistItem() = default;

    // 拷贝和移动构造函数
    PlaylistItem(const PlaylistItem&) = default;
    PlaylistItem(PlaylistItem&&) = default;
    PlaylistItem& operator=(const PlaylistItem&) = default;
    PlaylistItem& operator=(PlaylistItem&&) = default;

    // 文件名
    QString filename() const { return filename_; }
    void setFilename(const QString& filename) { filename_ = filename; }

    // 标题（显示名称）
    QString title() const { return title_; }
    void setTitle(const QString& title) { title_ = title; }

    // 时长（毫秒）
    int64_t duration() const { return duration_; }
    void setDuration(int64_t duration) { duration_ = duration; }

    // 格式化时长显示（mm:ss 或 hh:mm:ss）
    QString formattedDuration() const;

    // 是否有效（有文件名）
    bool isValid() const { return !filename_.isEmpty(); }

    // 相等比较
    bool operator==(const PlaylistItem& other) const {
        return filename_ == other.filename_;
    }
    bool operator!=(const PlaylistItem& other) const {
        return !(*this == other);
    }

private:
    QString filename_;      // 文件路径
    QString title_;         // 显示标题（可为空，默认从文件名提取）
    int64_t duration_ = 0;  // 时长（毫秒）
};

} // namespace playlist

// 注册到 Qt 元对象系统
Q_DECLARE_METATYPE(playlist::PlaylistItem)