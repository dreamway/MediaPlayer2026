# Logger 早期初始化修复实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 修复 macOS 启动崩溃问题，确保 logger 在使用前完成初始化。

**Architecture:** 调整 MainWindow 构造函数中的初始化顺序，先读取 LOG_MODE/LOG_LEVEL，再初始化 logger，最后读取完整配置。同时在 ApplicationSettings 中添加防御性空指针检查。

**Tech Stack:** C++17, Qt 6.6.3, spdlog

---

### Task 1: 修改 MainWindow 构造函数初始化顺序

**Files:**
- Modify: `WZMediaPlay/MainWindow.cpp:53-90`

**Step 1: 重构构造函数前半部分**

将构造函数从：
```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString startupConfig = readConfig();

    GlobalDef::getInstance()->VIDEO_FILE_TYPE << ".mp4" << ".flv" << ...

    if (GlobalDef::getInstance()->LOG_MODE == 1) {
#ifdef Q_OS_WIN
        AllocConsole();
        FILE *unused1, *unused2;
        freopen_s(&unused1, "CONOUT$", "w", stdout);
        freopen_s(&unused2, "CONOUT$", "w", stderr);
#endif
    }

    bool setupOk = setupLogger();
    if (false == setupOk) {
        QMessageBox::StandardButton button = QMessageBox::critical(static_cast<QWidget *>(this), QString(tr("Error")), QString(tr("Setup Logger Failed.")));
        std::cerr << "Logger setup failed.";
        exit(0);
    }

    logger->info("Setup completed. logger pointer value: {}", static_cast<void*>(logger));

    setWindowFlags(Qt::CustomizeWindowHint);

    ui.setupUi(this);

    logger->info("------------------ Begin Startup Config ----------------------------");
    logger->info(startupConfig.toUtf8().constData());
    logger->info("------------------ End Startup Config ----------------------------");
```

修改为：
```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. 先读取最小配置（包含 LOG_MODE 和 LOG_LEVEL）
    ApplicationSettings earlySettings;
    earlySettings.read_ApplicationGeneral();

    // 2. 初始化 logger（此时 LOG_MODE/LOG_LEVEL 已有值）
    if (GlobalDef::getInstance()->LOG_MODE == 1) {
#ifdef Q_OS_WIN
        AllocConsole();
        FILE *unused1, *unused2;
        freopen_s(&unused1, "CONOUT$", "w", stdout);
        freopen_s(&unused2, "CONOUT$", "w", stderr);
#endif
    }

    bool setupOk = setupLogger();
    if (false == setupOk) {
        QMessageBox::StandardButton button = QMessageBox::critical(nullptr, QString(tr("Error")), QString(tr("Setup Logger Failed.")));
        std::cerr << "Logger setup failed.";
        exit(0);
    }

    // 3. 读取完整配置
    QString startupConfig = readConfig();

    GlobalDef::getInstance()->VIDEO_FILE_TYPE << ".mp4" << ".flv" << ".f4v" << ".webm" << ".m4v" << ".mov" << ".3gp" << ".3g2" << ".rm" << ".rmvb" << ".wmv"
                                              << ".avi"
                                              << ".asf" << ".mpg" << ".mpeg" << ".mpe" << ".ts" << ".div" << ".dv" << ".divx" << ".vob" << ".mkv" << ".wzmp4"
                                              << ".wzavi"
                                              << ".wzmkv" << ".wzmov" << ".wzmpg";

    // 调试：验证logger初始化成功
    logger->info("Setup completed. logger pointer value: {}", static_cast<void*>(logger));

    setWindowFlags(Qt::CustomizeWindowHint);

    ui.setupUi(this);

    logger->info("------------------ Begin Startup Config ----------------------------");
    logger->info(startupConfig.toUtf8().constData());
    logger->info("------------------ End Startup Config ----------------------------");
```

**Step 2: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功，无错误

---

### Task 2: 修改 readConfig() 方法

**Files:**
- Modify: `WZMediaPlay/MainWindow.cpp:621-628`

**Step 1: 重构 readConfig() 方法**

将方法从：
```cpp
QString MainWindow::readConfig()
{
    ApplicationSettings appSettings;
    appSettings.read_ALL();
    appSettings.read_PlayList();

    return appSettings.ToString();
}
```

修改为：
```cpp
QString MainWindow::readConfig()
{
    ApplicationSettings appSettings;
    // read_ApplicationGeneral() 已在构造函数中提前调用
    appSettings.read_WindowSizeState();
    appSettings.read_PlayState();
    appSettings.read_Hotkey();
    appSettings.read_about_copyright();
    appSettings.read_PlayList();

    return appSettings.ToString();
}
```

**Step 2: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功，无错误

---

### Task 3: 添加 ApplicationSettings 中的防御性空指针检查

**Files:**
- Modify: `WZMediaPlay/ApplicationSettings.cpp:453-532` (read_PlayList)
- Modify: `WZMediaPlay/ApplicationSettings.cpp:534-577` (write_PlayList)
- Modify: `WZMediaPlay/ApplicationSettings.cpp:579-588` (clear_PlayList)
- Modify: `WZMediaPlay/ApplicationSettings.cpp:590-625` (load_PlayList)
- Modify: `WZMediaPlay/ApplicationSettings.cpp:627-651` (export_PlayList)

**Step 1: 修改 read_PlayList() 中的 logger 调用**

找到第 461 行：
```cpp
logger->warn("can't open json file...");
```

修改为：
```cpp
if (logger) {
    logger->warn("can't open json file...");
}
```

找到第 473 行：
```cpp
logger->error("Json 格式错误!: {}", int(jsonError.error));
```

修改为：
```cpp
if (logger) {
    logger->error("Json 格式错误!: {}", int(jsonError.error));
}
```

**Step 2: 修改 write_PlayList() 中的 logger 调用**

找到第 538 行：
```cpp
logger->error("can't open json file: {}", file.fileName().toStdString());
```

修改为：
```cpp
if (logger) {
    logger->error("can't open json file: {}", file.fileName().toStdString());
}
```

**Step 3: 修改 clear_PlayList() 中的 logger 调用**

找到第 583 行：
```cpp
logger->error("can't open json file.. {} ", file.fileName().toStdString());
```

修改为：
```cpp
if (logger) {
    logger->error("can't open json file.. {} ", file.fileName().toStdString());
}
```

**Step 4: 修改 load_PlayList() 中的 logger 调用**

找到第 594 行：
```cpp
logger->error("can't open json file.. {} ", file.fileName().toStdString());
```

修改为：
```cpp
if (logger) {
    logger->error("can't open json file.. {} ", file.fileName().toStdString());
}
```

找到第 606 行：
```cpp
logger->error("Json format wrong {} ", int(jsonError.error));
```

修改为：
```cpp
if (logger) {
    logger->error("Json format wrong {} ", int(jsonError.error));
}
```

**Step 5: 修改 export_PlayList() 中的 logger 调用**

找到第 632 行：
```cpp
logger->error("can't open json file.. {} ", file.fileName().toStdString());
```

修改为：
```cpp
if (logger) {
    logger->error("can't open json file.. {} ", file.fileName().toStdString());
}
```

**Step 6: 验证编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1 | head -50`
Expected: 编译成功，无错误

---

### Task 4: 验证修复效果

**Step 1: macOS 编译**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && cmake --build build 2>&1`
Expected: 编译成功

**Step 2: 运行程序验证启动**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026/build && ./WZMediaPlayer.app/Contents/MacOS/WZMediaPlayer &`
Expected: 程序启动成功，不再崩溃

**Step 3: 验证日志输出**

检查控制台或日志文件中是否有正常的启动日志：
```
Setup completed. logger pointer value: 0x...
------------------ Begin Startup Config ----------------------------
...
------------------ End Startup Config ----------------------------
```

---

### Task 5: 提交更改

**Step 1: 检查更改**

Run: `cd /Users/jingwenlai/leadwit-tech/WeiZheng/MediaPlayer2026 && git diff WZMediaPlay/MainWindow.cpp WZMediaPlay/ApplicationSettings.cpp`

**Step 2: 提交修复**

```bash
git add WZMediaPlay/MainWindow.cpp WZMediaPlay/ApplicationSettings.cpp docs/plans/2026-03-09-logger-early-init-fix-design.md docs/plans/2026-03-09-logger-early-init-fix.md
git commit -m "$(cat <<'EOF'
fix: 修复 macOS 启动崩溃问题 - logger 空指针访问

问题：MainWindow 构造函数中 readConfig() 在 setupLogger() 之前被调用，
导致 ApplicationSettings::read_PlayList() 中 logger->warn() 访问空指针。

修复：
1. 调整 MainWindow 构造函数初始化顺序，先读取 LOG_MODE/LOG_LEVEL，
   再初始化 logger，最后读取完整配置
2. 修改 readConfig() 方法，不再调用 read_ALL()
3. 在 ApplicationSettings 中所有使用 logger 的地方添加空指针检查

Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## 回归测试清单

- [ ] macOS 编译成功
- [ ] 程序启动不崩溃
- [ ] 日志正常输出
- [ ] 播放列表加载功能正常
- [ ] 配置文件读取正常