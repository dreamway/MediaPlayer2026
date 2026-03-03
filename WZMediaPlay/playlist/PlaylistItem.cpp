#include "PlaylistItem.h"
#include <QFileInfo>

namespace playlist {

PlaylistItem::PlaylistItem(const QString& filename)
    : filename_(filename)
{
    // 如果没有标题，从文件名提取
    if (title_.isEmpty() && !filename_.isEmpty()) {
        QFileInfo fileInfo(filename_);
        title_ = fileInfo.completeBaseName();  // 不包含扩展名的文件名
    }
}

PlaylistItem::PlaylistItem(const QString& filename, const QString& title, int64_t duration)
    : filename_(filename)
    , title_(title)
    , duration_(duration)
{
    // 如果标题为空，从文件名提取
    if (title_.isEmpty() && !filename_.isEmpty()) {
        QFileInfo fileInfo(filename_);
        title_ = fileInfo.completeBaseName();
    }
}

QString PlaylistItem::formattedDuration() const
{
    if (duration_ <= 0) {
        return "--:--";
    }

    int64_t totalSeconds = duration_ / 1000;
    int hours = static_cast<int>(totalSeconds / 3600);
    int minutes = static_cast<int>((totalSeconds % 3600) / 60);
    int seconds = static_cast<int>(totalSeconds % 60);

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

} // namespace playlist