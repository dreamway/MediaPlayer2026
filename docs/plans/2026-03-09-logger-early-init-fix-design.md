# Logger 早期初始化修复设计

**日期**: 2026-03-09
**状态**: 已批准
**优先级**: P0 (阻塞启动)

## 问题分析

### 崩溃堆栈
```
Thread 0 Crashed::  Dispatch queue: com.apple.main-thread
0   WZMediaPlayer    int std::__1::__cxx_atomic_load<int>(...)
1   WZMediaPlayer    std::__1::__atomic_base<int>::load(...) const
2   WZMediaPlayer    spdlog::logger::should_log(spdlog::level::level_enum) const
3   WZMediaPlayer    spdlog::logger::log(...)
4   WZMediaPlayer    void spdlog::logger::warn<char [24]>(...)
5   WZMediaPlayer    ApplicationSettings::read_PlayList() + 168
6   WZMediaPlayer    MainWindow::readConfig() + 60
7   WZMediaPlayer    MainWindow::MainWindow(QWidget*) + 436
```

### 根本原因
`MainWindow` 构造函数中初始化顺序错误：
- 第 56 行: `readConfig()` 调用，内部调用 `logger->warn()`
- 第 74 行: `setupLogger()` 才初始化 `logger`

此时 `logger` 为 `nullptr`，访问偏移 0x38 是 `spdlog::logger` 成员变量的偏移地址。

## 修复方案

### 方案选择
采用 **方案A: 早期初始化** - 在 `MainWindow` 构造函数中调整初始化顺序，将 logger 初始化提前到 `readConfig()` 之前。

### 改动清单

#### 1. MainWindow 构造函数 (MainWindow.cpp)

**修改前**:
```cpp
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    QString startupConfig = readConfig();  // 第56行 - 使用了 logger

    if (GlobalDef::getInstance()->LOG_MODE == 1) {
        // AllocConsole...
    }

    bool setupOk = setupLogger();  // 第74行
    // ...
}
```

**修改后**:
```cpp
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
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
    if (!setupOk) {
        QMessageBox::critical(nullptr, QString(tr("Error")), QString(tr("Setup Logger Failed.")));
        std::cerr << "Logger setup failed.";
        exit(0);
    }

    // 3. 读取完整配置
    QString startupConfig = readConfig();

    // 4. 后续初始化...
    GlobalDef::getInstance()->VIDEO_FILE_TYPE << ...;
    // ...
}
```

#### 2. readConfig() 方法 (MainWindow.cpp)

**修改前**:
```cpp
QString MainWindow::readConfig()
{
    ApplicationSettings appSettings;
    appSettings.read_ALL();
    appSettings.read_PlayList();
    return appSettings.ToString();
}
```

**修改后**:
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

#### 3. ApplicationSettings::read_PlayList() 防御性检查 (ApplicationSettings.cpp)

**修改前**:
```cpp
void ApplicationSettings::read_PlayList()
{
    QFile file(...);
    if (!file.open(QIODevice::ReadWrite)) {
        logger->warn("can't open json file...");  // 崩溃点
        return;
    }
    // ...
}
```

**修改后**:
```cpp
void ApplicationSettings::read_PlayList()
{
    QFile file(...);
    if (!file.open(QIODevice::ReadWrite)) {
        if (logger) {
            logger->warn("can't open json file...");
        }
        return;
    }
    // ...
}
```

#### 4. 同样添加防御性检查的位置

以下方法中也使用了 `logger`，需要添加空指针检查：

- `ApplicationSettings::write_PlayList()` (第 538 行)
- `ApplicationSettings::clear_PlayList()` (第 583 行)
- `ApplicationSettings::load_PlayList()` (第 594 行)
- `ApplicationSettings::export_PlayList()` (第 632 行)

## 测试验证

### 验证步骤
1. macOS 编译: `cmake -B build -G Ninja && cmake --build build`
2. 运行程序，确认不再崩溃
3. 验证日志正常输出

### 回归测试
- 验证播放列表加载功能正常
- 验证日志文件正确生成

## 影响范围

| 文件 | 改动类型 | 风险等级 |
|------|----------|----------|
| MainWindow.cpp | 重构初始化顺序 | 低 |
| ApplicationSettings.cpp | 添加空指针检查 | 低 |

## 后续建议

1. 考虑使用 RAII 模式管理 logger 生命周期
2. 在 CI 中添加 macOS 编译和启动测试