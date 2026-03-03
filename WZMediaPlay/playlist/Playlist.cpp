#include "Playlist.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <spdlog/spdlog.h>

extern spdlog::logger* logger;

namespace playlist {

Playlist::Playlist(QObject* parent)
    : QObject(parent)
    , randomGenerator_(QRandomGenerator::global()->generate())
{
    qRegisterMetaType<PlaylistItem>("PlaylistItem");
}

Playlist::~Playlist() = default;

void Playlist::addItem(const PlaylistItem& item)
{
    if (!item.isValid()) {
        return;
    }

    items_.append(item);
    int index = items_.count() - 1;
    emit itemAdded(index, item);
}

void Playlist::addItems(const QList<PlaylistItem>& items)
{
    for (const auto& item : items) {
        addItem(item);
    }
}

void Playlist::insertItem(int index, const PlaylistItem& item)
{
    if (!item.isValid() || index < 0 || index > items_.count()) {
        return;
    }

    items_.insert(index, item);
    
    // 调整当前索引
    if (currentIndex_ >= index) {
        currentIndex_++;
    }
    
    emit itemAdded(index, item);
}

void Playlist::removeItem(int index)
{
    if (index < 0 || index >= items_.count()) {
        return;
    }

    items_.removeAt(index);
    
    // 调整当前索引
    if (currentIndex_ == index) {
        currentIndex_ = -1;  // 当前项被删除
    } else if (currentIndex_ > index) {
        currentIndex_--;
    }
    
    emit itemRemoved(index);
}

void Playlist::removeItem(const PlaylistItem& item)
{
    int index = indexOf(item);
    if (index >= 0) {
        removeItem(index);
    }
}

void Playlist::clear()
{
    items_.clear();
    currentIndex_ = -1;
    randomHistory_.clear();
    emit playlistCleared();
}

PlaylistItem Playlist::itemAt(int index) const
{
    if (index < 0 || index >= items_.count()) {
        return PlaylistItem();
    }
    return items_.at(index);
}

int Playlist::indexOf(const PlaylistItem& item) const
{
    for (int i = 0; i < items_.count(); ++i) {
        if (items_.at(i) == item) {
            return i;
        }
    }
    return -1;
}

int Playlist::indexOf(const QString& filename) const
{
    for (int i = 0; i < items_.count(); ++i) {
        if (items_.at(i).filename() == filename) {
            return i;
        }
    }
    return -1;
}

PlaylistItem Playlist::currentItem() const
{
    if (currentIndex_ < 0 || currentIndex_ >= items_.count()) {
        return PlaylistItem();
    }
    return items_.at(currentIndex_);
}

void Playlist::setCurrentIndex(int index)
{
    if (index < -1 || index >= items_.count()) {
        return;
    }

    if (currentIndex_ != index) {
        currentIndex_ = index;
        emit currentItemChanged(currentIndex_, currentItem());
    }
}

bool Playlist::playNext()
{
    if (items_.isEmpty()) {
        return false;
    }

    int nextIndex = getNextIndex();
    if (nextIndex < 0 || nextIndex >= items_.count()) {
        return false;
    }

    setCurrentIndex(nextIndex);
    return true;
}

bool Playlist::playPrevious()
{
    if (items_.isEmpty()) {
        return false;
    }

    int prevIndex = getPreviousIndex();
    if (prevIndex < 0 || prevIndex >= items_.count()) {
        return false;
    }

    setCurrentIndex(prevIndex);
    return true;
}

bool Playlist::playFirst()
{
    if (items_.isEmpty()) {
        return false;
    }
    setCurrentIndex(0);
    return true;
}

bool Playlist::playLast()
{
    if (items_.isEmpty()) {
        return false;
    }
    setCurrentIndex(items_.count() - 1);
    return true;
}

void Playlist::setPlayMode(PlayMode mode)
{
    if (playMode_ != mode) {
        playMode_ = mode;
        
        // 切换模式时清空随机历史
        if (mode != PlayMode::Random) {
            randomHistory_.clear();
        }
        
        emit playModeChanged(mode);
    }
}

int Playlist::getNextIndex() const
{
    if (items_.isEmpty()) {
        return -1;
    }

    switch (playMode_) {
    case PlayMode::Sequential:
        if (currentIndex_ < 0) {
            return 0;
        }
        return (currentIndex_ + 1 < items_.count()) ? currentIndex_ + 1 : -1;

    case PlayMode::Loop:
        if (currentIndex_ < 0) {
            return 0;
        }
        return (currentIndex_ + 1) % items_.count();

    case PlayMode::Random:
        return getRandomIndex();

    case PlayMode::SingleLoop:
        return (currentIndex_ < 0) ? 0 : currentIndex_;

    default:
        return -1;
    }
}

int Playlist::getPreviousIndex() const
{
    if (items_.isEmpty()) {
        return -1;
    }

    // 随机模式下优先使用历史记录
    if (playMode_ == PlayMode::Random && !randomHistory_.isEmpty()) {
        return randomHistory_.last();
    }

    switch (playMode_) {
    case PlayMode::Sequential:
    case PlayMode::Loop:
    case PlayMode::Random:
        if (currentIndex_ <= 0) {
            return items_.count() - 1;  // 循环到最后一首
        }
        return currentIndex_ - 1;

    case PlayMode::SingleLoop:
        return (currentIndex_ < 0) ? 0 : currentIndex_;

    default:
        return -1;
    }
}

int Playlist::getRandomIndex() const
{
    if (items_.count() <= 1) {
        return 0;
    }

    int newIndex;
    do {
        newIndex = QRandomGenerator::global()->bounded(items_.count());
    } while (newIndex == currentIndex_);

    return newIndex;
}

bool Playlist::saveToFile(const QString& filename) const
{
    QJsonArray array;
    for (const auto& item : items_) {
        QJsonObject obj;
        obj["filename"] = item.filename();
        obj["title"] = item.title();
        obj["duration"] = static_cast<qint64>(item.duration());
        array.append(obj);
    }

    QJsonDocument doc(array);
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        SPDLOG_LOGGER_ERROR(logger, "Playlist::saveToFile: Failed to open file: {}", filename.toStdString());
        return false;
    }

    file.write(doc.toJson());
    file.close();
    return true;
}

bool Playlist::loadFromFile(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        SPDLOG_LOGGER_ERROR(logger, "Playlist::loadFromFile: Failed to open file: {}", filename.toStdString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        SPDLOG_LOGGER_ERROR(logger, "Playlist::loadFromFile: Invalid JSON format");
        return false;
    }

    clear();
    QJsonArray array = doc.array();
    for (const auto& value : array) {
        if (!value.isObject()) continue;
        
        QJsonObject obj = value.toObject();
        PlaylistItem item;
        item.setFilename(obj["filename"].toString());
        item.setTitle(obj["title"].toString());
        item.setDuration(obj["duration"].toVariant().toLongLong());
        
        addItem(item);
    }

    return true;
}

QList<int> Playlist::findByFilename(const QString& pattern) const
{
    QList<int> result;
    QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
    
    for (int i = 0; i < items_.count(); ++i) {
        if (items_.at(i).filename().contains(regex)) {
            result.append(i);
        }
    }
    return result;
}

QList<int> Playlist::findByTitle(const QString& pattern) const
{
    QList<int> result;
    QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
    
    for (int i = 0; i < items_.count(); ++i) {
        if (items_.at(i).title().contains(regex)) {
            result.append(i);
        }
    }
    return result;
}

} // namespace playlist
