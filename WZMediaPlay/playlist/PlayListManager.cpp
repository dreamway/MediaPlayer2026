#include "PlayListManager.h"
#include "GlobalDef.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QStringConverter>
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

// 获取配置文件路径（与 ApplicationSettings.cpp 中相同的逻辑）
static QString getPlaylistConfigPath()
{
#ifdef Q_OS_MAC
    // macOS app bundle: 配置文件在 Contents/config/
    return QCoreApplication::applicationDirPath() + "/../config";
#else
    // Windows/Linux: 配置文件在应用程序目录下的 config/
    return QCoreApplication::applicationDirPath() + "/config";
#endif
}

PlayListManager::PlayListManager(QObject *parent)
    : QObject(parent)
{
    // 默认 JSON 文件路径
    jsonFilePath_ = getPlaylistConfigPath() + "/playList.json";

    // 从 JSON 文件直接加载作为单一数据源
    loadFromJson(jsonFilePath_, true);
}

PlayListManager::~PlayListManager()
{
    // 清理所有播放列表
    for (auto it = playlists_.begin(); it != playlists_.end(); ++it) {
        delete it.value();
    }
    playlists_.clear();
}

playlist::Playlist* PlayListManager::getPlaylist(int playListIndex) const
{
    auto it = playlists_.find(playListIndex);
    if (it == playlists_.end()) {
        return nullptr;
    }
    return it.value();
}

bool PlayListManager::addVideoToPlayList(const QString &videoPath, int playListIndex)
{
    // 确保播放列表存在
    if (!playlists_.contains(playListIndex)) {
        playlists_[playListIndex] = new playlist::Playlist();
        // 设置播放模式
        playlists_[playListIndex]->setPlayMode(static_cast<playlist::Playlist::PlayMode>(playMode_));
    }

    auto* playlist = playlists_[playListIndex];

    // 检查是否已存在
    for (int i = 0; i < playlist->count(); ++i) {
        if (playlist->itemAt(i).filename() == videoPath) {
            if (logger) {
                logger->debug("PlayListManager::addVideoToPlayList: Video already exists: {}", videoPath.toStdString());
            }
            return false;
        }
    }

    // 添加视频，同时检查文件存在性
    playlist::PlaylistItem item(videoPath);
    bool fileExists = playlist::PlaylistItem::checkFileExists(videoPath);
    item.setFileExists(fileExists);

    if (!fileExists && logger) {
        logger->warn("PlayListManager::addVideoToPlayList: File does not exist: {}", videoPath.toStdString());
    }

    playlist->addItem(item);

    // 标记数据已变化
    markDirty();

    // 同步到 PlayListData（向后兼容）
    syncToGlobalDef();

    emit playListChanged(playListIndex);
    return true;
}

bool PlayListManager::removeVideoFromPlayList(int videoIndex, int playListIndex)
{
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist || videoIndex < 0 || videoIndex >= playlist->count()) {
        return false;
    }

    playlist->removeItem(videoIndex);

    // 标记数据已变化
    markDirty();

    // 同步到 PlayListData
    syncToGlobalDef();

    emit playListChanged(playListIndex);
    return true;
}

bool PlayListManager::clearPlayList(int playListIndex)
{
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist) {
        return false;
    }

    playlist->clear();

    // 标记数据已变化
    markDirty();

    // 同步到 PlayListData
    syncToGlobalDef();

    emit playListChanged(playListIndex);
    return true;
}

int PlayListManager::getPlayListSize(int playListIndex) const
{
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist) {
        return 0;
    }
    return playlist->count();
}

QString PlayListManager::getVideoPath(int videoIndex, int playListIndex) const
{
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist || videoIndex < 0 || videoIndex >= playlist->count()) {
        return QString();
    }
    return playlist->itemAt(videoIndex).filename();
}

int PlayListManager::getCurrentVideoIndex(int playListIndex) const
{
    if (playListIndex == currentPlayListIndex_) {
        return currentVideoIndex_;
    }
    return -1;
}

void PlayListManager::setCurrentVideo(int videoIndex, int playListIndex, bool emitSignal)
{
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist || videoIndex < 0 || videoIndex >= playlist->count()) {
        if (logger) {
            logger->warn("PlayListManager::setCurrentVideo: Invalid index: videoIndex={}, playListIndex={}", videoIndex, playListIndex);
        }
        return;
    }

    updateCurrentVideoIndex(videoIndex, playListIndex);

    // 更新内部 Playlist 的当前索引
    playlist->setCurrentIndex(videoIndex);

    // 只有在明确需要时才发送信号，避免与openPath()重复触发
    if (emitSignal) {
        QString videoPath = getVideoPath(videoIndex, playListIndex);
        emit currentVideoChanged(videoPath);
    }
}

QString PlayListManager::getCurrentVideoPath() const
{
    return getVideoPath(currentVideoIndex_, currentPlayListIndex_);
}

bool PlayListManager::hasNextVideo() const
{
    auto* playlist = getPlaylist(currentPlayListIndex_);
    if (!playlist) {
        return false;
    }

    // 根据播放模式判断
    switch (playMode_) {
    case PlayMode::Sequential:
        return currentVideoIndex_ >= 0 && currentVideoIndex_ < playlist->count() - 1;
    case PlayMode::Loop:
    case PlayMode::Random:
    case PlayMode::SingleLoop:
        return playlist->count() > 0;
    }
    return false;
}

bool PlayListManager::hasPreviousVideo() const
{
    auto* playlist = getPlaylist(currentPlayListIndex_);
    if (!playlist) {
        return false;
    }

    // 根据播放模式判断
    switch (playMode_) {
    case PlayMode::Sequential:
        return currentVideoIndex_ > 0;
    case PlayMode::Loop:
    case PlayMode::Random:
    case PlayMode::SingleLoop:
        return playlist->count() > 0;
    }
    return false;
}

QString PlayListManager::getNextVideoPath() const
{
    auto* playlist = getPlaylist(currentPlayListIndex_);
    if (!playlist) {
        return QString();
    }

    int nextIndex = -1;

    switch (playMode_) {
    case PlayMode::Sequential:
    case PlayMode::Loop:
        if (currentVideoIndex_ < playlist->count() - 1) {
            nextIndex = currentVideoIndex_ + 1;
        } else if (playMode_ == PlayMode::Loop) {
            nextIndex = 0;
        }
        break;
    case PlayMode::Random:
        nextIndex = playlist->indexOf(playlist->currentItem());
        // 使用内部 Playlist 的随机逻辑
        if (playlist->playNext()) {
            nextIndex = playlist->currentIndex();
        }
        break;
    case PlayMode::SingleLoop:
        nextIndex = currentVideoIndex_;
        break;
    }

    if (nextIndex >= 0 && nextIndex < playlist->count()) {
        return playlist->itemAt(nextIndex).filename();
    }
    return QString();
}

QString PlayListManager::getPreviousVideoPath() const
{
    auto* playlist = getPlaylist(currentPlayListIndex_);
    if (!playlist) {
        return QString();
    }

    int prevIndex = -1;

    switch (playMode_) {
    case PlayMode::Sequential:
    case PlayMode::Loop:
        if (currentVideoIndex_ > 0) {
            prevIndex = currentVideoIndex_ - 1;
        } else if (playMode_ == PlayMode::Loop) {
            prevIndex = playlist->count() - 1;
        }
        break;
    case PlayMode::Random:
        // 随机模式下使用历史记录
        if (playlist->playPrevious()) {
            prevIndex = playlist->currentIndex();
        }
        break;
    case PlayMode::SingleLoop:
        prevIndex = currentVideoIndex_;
        break;
    }

    if (prevIndex >= 0 && prevIndex < playlist->count()) {
        return playlist->itemAt(prevIndex).filename();
    }
    return QString();
}

bool PlayListManager::switchToNextVideo()
{
    auto* playlist = getPlaylist(currentPlayListIndex_);
    if (!playlist) {
        return false;
    }

    // 设置播放模式
    playlist->setPlayMode(static_cast<playlist::Playlist::PlayMode>(playMode_));
    playlist->setCurrentIndex(currentVideoIndex_);

    if (!playlist->playNext()) {
        return false;
    }

    int nextIndex = playlist->currentIndex();
    setCurrentVideo(nextIndex, currentPlayListIndex_, true);
    return true;
}

bool PlayListManager::switchToPreviousVideo()
{
    auto* playlist = getPlaylist(currentPlayListIndex_);
    if (!playlist) {
        return false;
    }

    // 设置播放模式
    playlist->setPlayMode(static_cast<playlist::Playlist::PlayMode>(playMode_));
    playlist->setCurrentIndex(currentVideoIndex_);

    if (!playlist->playPrevious()) {
        return false;
    }

    int prevIndex = playlist->currentIndex();
    setCurrentVideo(prevIndex, currentPlayListIndex_, true);
    return true;
}

bool PlayListManager::switchToVideo(int index, int playListIndex)
{
    if (!isValidIndex(index, playListIndex)) {
        return false;
    }

    setCurrentVideo(index, playListIndex, true);
    return true;
}

void PlayListManager::setPlayMode(PlayMode mode)
{
    if (playMode_ != mode) {
        playMode_ = mode;

        // 更新所有播放列表的播放模式
        for (auto& playlist : playlists_) {
            playlist->setPlayMode(static_cast<playlist::Playlist::PlayMode>(mode));
        }

        emit playModeChanged(mode);
    }
}

void PlayListManager::loadFromPlayListData(const PlayListData& data)
{
    // 清理旧的播放列表
    for (auto it = playlists_.begin(); it != playlists_.end(); ++it) {
        delete it.value();
    }
    playlists_.clear();

    for (int i = 0; i < data.play_list.size(); ++i) {
        auto* playlist = new playlist::Playlist();

        for (const auto& video : data.play_list[i].video_list) {
            // 创建播放项并检查文件存在性
            playlist::PlaylistItem item(video.video_path);
            item.setFileExists(playlist::PlaylistItem::checkFileExists(video.video_path));
            playlist->addItem(item);
        }

        playlists_[i] = playlist;
    }

    currentPlayListIndex_ = data.playlist_current_index;
    currentVideoIndex_ = data.playlist_current_video;
    playListData_ = data;
}

void PlayListManager::saveToPlayListData(PlayListData& data) const
{
    data.play_list.clear();

    // 使用 QMap 的有序迭代（按键升序），保证顺序一致
    for (auto it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
        PlayList playlistInfo;
        playlistInfo.list_name = QString("Playlist %1").arg(it.key());

        auto* playlist = it.value();
        for (int i = 0; i < playlist->count(); ++i) {
            VideoInfo video;
            video.video_path = playlist->itemAt(i).filename();
            video.file_exists = playlist->itemAt(i).fileExists();  // 同步文件存在状态
            playlistInfo.video_list.append(video);
        }

        data.play_list.append(playlistInfo);
    }

    data.playlist_current_index = currentPlayListIndex_;
    data.playlist_current_video = currentVideoIndex_;
}

bool PlayListManager::isValidIndex(int videoIndex, int playListIndex) const
{
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist) {
        return false;
    }

    return videoIndex >= 0 && videoIndex < playlist->count();
}

void PlayListManager::updateCurrentVideoIndex(int videoIndex, int playListIndex)
{
    currentPlayListIndex_ = playListIndex;
    currentVideoIndex_ = videoIndex;

    // 同步到 PlayListData
    playListData_.playlist_current_index = playListIndex;
    playListData_.playlist_current_video = videoIndex;
}

void PlayListManager::syncToGlobalDef()
{
    saveToPlayListData(playListData_);
    GlobalDef::getInstance()->PLAY_LIST_DATA = playListData_;
}

// ========== 新增方法实现 ==========

bool PlayListManager::loadFromJson(const QString& jsonFilePath, bool validateFiles)
{
    QString filePath = jsonFilePath.isEmpty() ? jsonFilePath_ : jsonFilePath;
    if (jsonFilePath.isEmpty()) {
        jsonFilePath_ = filePath;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->warn("PlayListManager::loadFromJson: Can't open json file: {}", filePath.toStdString());
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString jsonStr = stream.readAll();
    file.close();

    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError && !jsonDoc.isNull()) {
        if (logger) {
            logger->error("PlayListManager::loadFromJson: Json format error: {}", static_cast<int>(jsonError.error));
        }
        return false;
    }

    QJsonObject rootObj = jsonDoc.object();

    // 解析当前播放信息
    int currentIndex = rootObj.value("playlist_current_index").toInt(0);
    int currentVideo = rootObj.value("playlist_current_video").toInt(-1);
    int64_t playTime = rootObj.value("playlist_video_play_time").toVariant().toLongLong();

    // 清理旧的播放列表
    for (auto it = playlists_.begin(); it != playlists_.end(); ++it) {
        delete it.value();
    }
    playlists_.clear();

    // 解析播放列表数组
    QJsonValue playlistListArray = rootObj.value("playlist_list_array");
    if (playlistListArray.type() == QJsonValue::Array) {
        QJsonArray jsonArray = playlistListArray.toArray();

        for (int i = 0; i < jsonArray.size(); ++i) {
            QJsonObject playlistObj = jsonArray[i].toObject();
            auto* playlist = new playlist::Playlist();

            // 解析视频列表
            QJsonValue videoListValue = playlistObj.value("video_array");
            if (videoListValue.type() == QJsonValue::Array) {
                QJsonArray videoArray = videoListValue.toArray();
                for (int j = 0; j < videoArray.size(); ++j) {
                    QJsonObject videoObj = videoArray[j].toObject();
                    QString videoPath = videoObj.value("video_path").toString();

                    // 创建播放项
                    playlist::PlaylistItem item(videoPath);

                    // 从 JSON 读取文件存在状态（如果存在）
                    bool savedFileExists = videoObj.value("file_exists").toBool(true);  // 默认 true（向后兼容）
                    item.setFileExists(savedFileExists);

                    // 验证文件存在性（如果需要，会覆盖 JSON 中的状态）
                    if (validateFiles) {
                        bool exists = playlist::PlaylistItem::checkFileExists(videoPath);
                        item.setFileExists(exists);
                        if (!exists && logger) {
                            logger->debug("PlayListManager::loadFromJson: File not found: {}", videoPath.toStdString());
                        }
                    }

                    playlist->addItem(item);
                }
            }

            playlists_[i] = playlist;
        }
    }

    // 确保至少有一个播放列表
    if (playlists_.isEmpty()) {
        playlists_[0] = new playlist::Playlist();
    }

    currentPlayListIndex_ = currentIndex;
    currentVideoIndex_ = currentVideo;

    // 更新 PlayListData（向后兼容）
    playListData_.playlist_current_index = currentIndex;
    playListData_.playlist_current_video = currentVideo;
    playListData_.playlist_video_play_time = playTime;
    playListData_.play_list.clear();

    for (auto it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
        PlayList pl;
        pl.list_name = QString("Playlist %1").arg(it.key());
        auto* playlist = it.value();
        for (int i = 0; i < playlist->count(); ++i) {
            VideoInfo vi;
            vi.video_path = playlist->itemAt(i).filename();
            vi.file_exists = playlist->itemAt(i).fileExists();  // 同步文件存在状态
            pl.video_list.append(vi);
        }
        playListData_.play_list.append(pl);
    }

    // 同步到 GlobalDef（向后兼容）
    GlobalDef::getInstance()->PLAY_LIST_DATA = playListData_;

    // 初始加载后标记为干净
    isDirty_ = false;

    // 验证文件存在性并发出信号
    if (validateFiles) {
        QStringList missingFiles = validateFileExistence();
        if (!missingFiles.isEmpty()) {
            emit fileValidationComplete(missingFiles);
        }
    }

    if (logger) {
        logger->info("PlayListManager::loadFromJson: Loaded {} playlists from {}",
                    playlists_.size(), filePath.toStdString());
    }

    return true;
}

bool PlayListManager::saveToJson(const QString& jsonFilePath)
{
    QString filePath = jsonFilePath.isEmpty() ? jsonFilePath_ : jsonFilePath;

    QJsonObject rootObj;

    // 写入当前播放信息
    rootObj.insert("playlist_current_index", currentPlayListIndex_);
    rootObj.insert("playlist_current_video", currentVideoIndex_);
    rootObj.insert("playlist_video_play_time", QJsonValue::fromVariant(QVariant::fromValue(playListData_.playlist_video_play_time)));

    // 写入播放列表数组（有序）
    QJsonArray playlistArray;

    for (auto it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
        QJsonObject playlistObj;
        playlistObj.insert("playlist_list_name", QString("Playlist %1").arg(it.key()));

        QJsonArray videoArray;
        auto* playlist = it.value();
        for (int i = 0; i < playlist->count(); ++i) {
            QJsonObject videoObj;
            videoObj.insert("video_path", playlist->itemAt(i).filename());
            videoObj.insert("video_time", QString());
            videoObj.insert("file_exists", playlist->itemAt(i).fileExists());  // 保存文件存在状态
            videoArray.append(videoObj);
        }

        playlistObj.insert("video_array", videoArray);
        playlistArray.append(playlistObj);
    }

    rootObj.insert("playlist_list_array", playlistArray);

    // 写入文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (logger) {
            logger->error("PlayListManager::saveToJson: Can't open file for writing: {}", filePath.toStdString());
        }
        return false;
    }

    QJsonDocument jsonDoc(rootObj);
    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();

    // 保存成功后标记为干净
    isDirty_ = false;

    if (logger) {
        logger->info("PlayListManager::saveToJson: Saved {} playlists to {}",
                    playlists_.size(), filePath.toStdString());
    }

    return true;
}

QStringList PlayListManager::getMissingFiles(int playListIndex) const
{
    QStringList missingFiles;
    auto* playlist = getPlaylist(playListIndex);
    if (!playlist) {
        return missingFiles;
    }

    for (int i = 0; i < playlist->count(); ++i) {
        const auto& item = playlist->itemAt(i);
        if (!item.fileExists()) {
            missingFiles.append(item.filename());
        }
    }

    return missingFiles;
}

QList<int> PlayListManager::getPlaylistIndices() const
{
    QList<int> indices;
    for (auto it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
        indices.append(it.key());
    }
    return indices;
}

QStringList PlayListManager::validateFileExistence()
{
    QStringList allMissingFiles;

    for (auto it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
        auto* playlist = it.value();
        for (int i = 0; i < playlist->count(); ++i) {
            // itemAt 返回值类型，我们需要检查 fileExists 状态
            const playlist::PlaylistItem item = playlist->itemAt(i);
            QString path = item.filename();

            // 重新检查文件存在性
            bool exists = playlist::PlaylistItem::checkFileExists(path);

            if (!exists) {
                allMissingFiles.append(path);
                if (logger) {
                    logger->debug("PlayListManager::validateFileExistence: Missing file: {}", path.toStdString());
                }
            }
        }
    }

    return allMissingFiles;
}

int PlayListManager::removeMissingFiles()
{
    int removedCount = 0;

    // 从后向前遍历，避免删除时索引变化问题
    for (auto it = playlists_.begin(); it != playlists_.end(); ++it) {
        auto* playlist = it.value();
        for (int i = playlist->count() - 1; i >= 0; --i) {
            const playlist::PlaylistItem item = playlist->itemAt(i);
            if (!item.fileExists()) {
                playlist->removeItem(i);
                removedCount++;
                if (logger) {
                    logger->debug("PlayListManager::removeMissingFiles: Removed missing file: {}", item.filename().toStdString());
                }
            }
        }
    }

    if (removedCount > 0) {
        markDirty();
        syncToGlobalDef();
    }

    return removedCount;
}

int PlayListManager::getTotalMissingFilesCount() const
{
    int count = 0;
    for (auto it = playlists_.constBegin(); it != playlists_.constEnd(); ++it) {
        auto* playlist = it.value();
        for (int i = 0; i < playlist->count(); ++i) {
            if (!playlist->itemAt(i).fileExists()) {
                count++;
            }
        }
    }
    return count;
}