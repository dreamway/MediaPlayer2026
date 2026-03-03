#include "PlayListManager.h"
#include "GlobalDef.h"
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

PlayListManager::PlayListManager(QObject *parent)
    : QObject(parent)
{
    // 从GlobalDef获取初始数据
    playListData_ = GlobalDef::getInstance()->PLAY_LIST_DATA;
    
    // 加载数据到内部 Playlist
    loadFromPlayListData(playListData_);
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
    
    // 添加视频
    playlist->addItem(playlist::PlaylistItem(videoPath));
    
    // 同步到 PlayListData
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
            playlist->addItem(playlist::PlaylistItem(video.video_path));
        }
        
        playlists_[i] = playlist;
    }
    
    currentPlayListIndex_ = data.playlist_current_index;
    currentVideoIndex_ = data.playlist_current_video;
}

void PlayListManager::saveToPlayListData(PlayListData& data) const
{
    data.play_list.clear();
    
    for (auto it = playlists_.begin(); it != playlists_.end(); ++it) {
        PlayList playlistInfo;
        playlistInfo.list_name = QString("Playlist %1").arg(it.key());
        
        auto* playlist = it.value();
        for (int i = 0; i < playlist->count(); ++i) {
            VideoInfo video;
            video.video_path = playlist->itemAt(i).filename();
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
