# 添加的快捷键用于自动化测试

## 添加的快捷键到 MainWindow.cpp

### 1. 添加成员变量到 MainWindow.h
```cpp
// 在 MainWindow 类中添加
private:
    QShortcut *shortcut_SeekLeft = nullptr;
    QShortcut *shortcut_SeekRight = nullptr;
    QShortcut *shortcut_SeekLeftLarge = nullptr;
    QShortcut *shortcut_SeekRightLarge = nullptr;
```

### 2. 在 MainWindow.cpp 的 initHotKey() 函数中添加（约line 3428后）
```cpp
//    Seek Tab (新增，用于自动化测试)
    if (shortcut_SeekLeft == nullptr) {
        shortcut_SeekLeft = new QShortcut(QKeySequence("Left"), this);
        connect(shortcut_SeekLeft, &QShortcut::activated, this, &MainWindow::onSeekLeftKey);
    }

    if (shortcut_SeekRight == nullptr) {
        shortcut_SeekRight = new QShortcut(QKeySequence("Right"), this);
        connect(shortcut_SeekRight, &QShortcut::activated, this, &MainWindow::onSeekRightKey);
    }

    if (shortcut_SeekLeftLarge == nullptr) {
        shortcut_SeekLeftLarge = new QShortcut(QKeySequence("Ctrl+Left"), this);
        connect(shortcut_SeekLeftLarge, &QShortcut::activated, this, &MainWindow::onSeekLeftLargeKey);
    }

    if (shortcut_SeekRightLarge == nullptr) {
        shortcut_SeekRightLarge = new QShortcut(QKeySequence("Ctrl+Right"), this);
        connect(shortcut_SeekRightLarge, &QShortcut::activated, this, &MainWindow::onSeekRightLargeKey);
    }
```

### 3. 在 MainWindow.cpp 中添加槽函数（在 MainWindow 类中）
```cpp
// Seek左（少量seek）
void MainWindow::onSeekLeftKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间-5秒
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t seekPosMs = std::max(0LL, currentPosMs - 5000);
        playController_->seek(seekPosMs / 1000.0);
    }
}

// Seek右（少量seek）
void MainWindow::onSeekRightKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间+5秒
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t durationMs = playController_->getDurationMs();
        int64_t seekPosMs = std::min(durationMs - 1000, currentPosMs + 5000);
        playController_->seek(seekPosMs / 1000.0);
    }
}

// Seek左大量（10%seek）
void MainWindow::onSeekLeftLargeKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间-10%
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t durationMs = playController_->getDurationMs();
        int64_t seekPosMs = std::max(0LL, currentPosMs - durationMs / 10);
        playController_->seek(seekPosMs / 1000.0);
    }
}

// Seek右大量（10%seek）
void MainWindow::onSeekRightLargeKey()
{
    if (playController_ && playController_->isOpened()) {
        // seek当前时间+10%
        int64_t currentPosMs = playController_->getCurrentPositionMs();
        int64_t durationMs = playController_->getDurationMs();
        int64_t seekPosMs = std::min(durationMs - 1000, currentPosMs + durationMs / 10);
        playController_->seek(seekPosMs / 1000.0);
    }
}
```

### 4. 在 MainWindow.h 中添加槽函数声明
```cpp
private slots:
    void onSeekLeftKey();
    void onSeekRightKey();
    void onSeekLeftLargeKey();
    void onSeekRightLargeKey();
```

### 5. 需要包含的头文件（MainWindow.cpp已经包含）
```cpp
#include <chrono>
```
