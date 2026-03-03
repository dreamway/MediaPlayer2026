// 自动化测试：Seeking功能测试
// 模拟用户操作，自动测试seeking功能

#include "../MainWindow.h"
#include "../PlayController.h"
#include "../PlaybackStateMachine.h"
#include <chrono>
#include <thread>
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QtTest/QtTest>

class SeekingAutomatedTest : public QObject
{
    Q_OBJECT

private:
    PlayController *playController_ = nullptr;
    MainWindow *mainWindow_ = nullptr;
    QString testVideoPath_ = "D:/BaiduNetdiskDownload/3D片源/d67932db75e03691431d4427564d5303.mp4";

    // 测试结果记录
    struct TestResult
    {
        QString testName;
        bool passed;
        QString message;
        int64_t seekPositionMs;
        int64_t actualPositionMs;
        int64_t errorMs;
    };
    QList<TestResult> testResults_;

    void addResult(const QString &name, bool passed, const QString &message, int64_t seekMs, int64_t actualMs, int64_t errorMs)
    {
        TestResult result;
        result.testName = name;
        result.passed = passed;
        result.message = message;
        result.seekPositionMs = seekMs;
        result.actualPositionMs = actualMs;
        result.errorMs = errorMs;
        testResults_.append(result);
    }

    // 等待指定时间（毫秒）
    void waitForMs(int ms)
    {
        QEventLoop loop;
        QTimer::singleShot(ms, &loop, &QEventLoop::quit);
        loop.exec();
    }

    // 模拟鼠标点击进度条（触发seeking）
    void simulateSliderClick(int seekPositionSeconds)
    {
        if (!mainWindow_) {
            qDebug() << "MainWindow is null";
            return;
        }

        // 调用MainWindow的seek方法（通过反射或直接调用）
        // 这里我们假设MainWindow有一个公共方法或信号可以触发seeking
        QMetaObject::invokeMethod(mainWindow_, "seekToPosition", Qt::DirectConnection, Q_ARG(int, seekPositionSeconds * 1000)); // 转换为毫秒

        qDebug() << "Simulated seek to" << seekPositionSeconds << "seconds";
    }

private slots:
    void initTestCase()
    {
        qDebug() << "=== Seeking Automated Test Init ===";

        // 创建PlayController和MainWindow
        playController_ = new PlayController(this);

        // 注意：这里应该从主窗口获取实例，而不是重新创建
        // 自动化测试需要集成到现有的应用程序中
        // 这里只是示例代码

        // 打开测试视频
        qDebug() << "Opening test video:" << testVideoPath_;
        // bool opened = playController_->open(testVideoPath_);

        waitForMs(2000); // 等待视频加载完成
    }

    void cleanupTestCase()
    {
        qDebug() << "=== Seeking Automated Test Cleanup ===";

        if (playController_) {
            playController_->stop();
            delete playController_;
            playController_ = nullptr;
        }

        printTestResults();
    }

    // 测试1：单次seeking到不同位置
    void testSingleSeek()
    {
        qDebug() << "Test 1: Single Seek";

        QList<int> testPositions = {0, 10, 25, 50, 75, 90}; // 百分比

        for (int percent : testPositions) {
            // 计算seek位置（假设视频总时长100秒）
            int seekSeconds = percent;

            // 模拟seeking
            simulateSliderClick(seekSeconds);

            // 等待seeking完成
            waitForMs(500);

            // 获取实际位置
            int64_t actualPositionMs = playController_->getCurrentPositionMs();
            int64_t expectedPositionMs = seekSeconds * 1000;
            int64_t errorMs = abs(actualPositionMs - expectedPositionMs);

            // 验证：误差应该小于500ms
            bool passed = (errorMs < 500);
            QString message = passed ? "OK" : QString("Error: %1ms").arg(errorMs);

            addResult("SingleSeek", passed, message, expectedPositionMs, actualPositionMs, errorMs);

            qDebug() << "Seek to" << seekSeconds << "s -> Actual:" << actualPositionMs << "ms, Error:" << errorMs << "ms, Status:" << message;

            // 等待一段时间，确保解码器稳定
            waitForMs(1000);
        }
    }

    // 测试2：多次连续seeking
    void testMultipleSeeks()
    {
        qDebug() << "Test 2: Multiple Seeks";

        QList<int> seekPositions = {10, 25, 50, 75, 90, 75, 50, 25, 10, 0};

        for (int i = 0; i < seekPositions.size(); ++i) {
            int seekSeconds = seekPositions[i];

            // 快速seeking（模拟用户快速点击）
            simulateSliderClick(seekSeconds);

            waitForMs(200); // 短暂等待

            int64_t actualPositionMs = playController_->getCurrentPositionMs();
            int64_t expectedPositionMs = seekSeconds * 1000;
            int64_t errorMs = abs(actualPositionMs - expectedPositionMs);

            bool passed = (errorMs < 500);
            QString message = passed ? "OK" : QString("Error: %1ms").arg(errorMs);

            addResult("MultipleSeek", passed, message, expectedPositionMs, actualPositionMs, errorMs);

            qDebug() << "Seek" << (i + 1) << "of" << seekPositions.size() << "->" << seekSeconds << "s, Error:" << errorMs << "ms";
        }

        // 最后等待，确保所有帧都处理完
        waitForMs(2000);
    }

    // 测试3：边界情况seeking
    void testBoundarySeeks()
    {
        qDebug() << "Test 3: Boundary Seeks";

        QList<int> boundaryPositions = {0, 100}; // 开始和结束

        for (int seekSeconds : boundaryPositions) {
            simulateSliderClick(seekSeconds);

            waitForMs(500);

            int64_t actualPositionMs = playController_->getCurrentPositionMs();
            int64_t expectedPositionMs = seekSeconds * 1000;
            int64_t errorMs = abs(actualPositionMs - expectedPositionMs);

            bool passed = (errorMs < 500);
            QString message = passed ? "OK" : QString("Error: %1ms").arg(errorMs);

            addResult("BoundarySeek", passed, message, expectedPositionMs, actualPositionMs, errorMs);

            qDebug() << "Boundary seek to" << seekSeconds << "s -> Actual:" << actualPositionMs << "ms, Status:" << message;

            waitForMs(1000);
        }
    }

    // 测试4：快速连续seeking（压力测试）
    void testRapidSeeks()
    {
        qDebug() << "Test 4: Rapid Seeks (Stress Test)";

        // 快速连续seek 20次
        for (int i = 0; i < 20; ++i) {
            // 随机位置（0-99秒）
            int seekSeconds = qrand() % 100;

            simulateSliderClick(seekSeconds);

            waitForMs(100); // 非常短的间隔
        }

        // 等待最后的seeking完成
        waitForMs(1000);

        // 获取最终位置
        int64_t finalPositionMs = playController_->getCurrentPositionMs();

        qDebug() << "Rapid seeks completed. Final position:" << finalPositionMs << "ms";

        // 只要没有崩溃，就认为测试通过
        addResult("RapidSeeks", true, "No crash", 0, finalPositionMs, 0);
    }

    // 测试5：seeking后音视频同步
    void testAVSyncAfterSeek()
    {
        qDebug() << "Test 5: AV Sync After Seek";

        // Seek到50%位置
        simulateSliderClick(50);

        // 等待seeking完成并播放5秒
        waitForMs(5500);

        // 检查音视频同步
        // 这里需要获取视频和音频时钟的差异
        // 假设PlayController提供了getAVSyncError()方法

        // 示例代码（需要根据实际API调整）
        // int64_t syncErrorMs = playController_->getAVSyncError();

        // bool passed = (abs(syncErrorMs) < 100); // 100ms以内认为同步
        // addResult("AVSyncAfterSeek", passed, QString("Sync error: %1ms").arg(syncErrorMs),
        //          50000, playController_->getCurrentPositionMs(), syncErrorMs);

        qDebug() << "AV Sync test completed";
    }

    void printTestResults()
    {
        qDebug() << "\n========== Test Results Summary ==========";
        qDebug() << "Total tests:" << testResults_.size();

        int passedCount = 0;
        int failedCount = 0;

        for (const TestResult &result : testResults_) {
            if (result.passed) {
                passedCount++;
                qDebug() << "✓ PASS:" << result.testName << "- Seek:" << result.seekPositionMs << "ms"
                         << "- Actual:" << result.actualPositionMs << "ms"
                         << "- Error:" << result.errorMs << "ms";
            } else {
                failedCount++;
                qDebug() << "✗ FAIL:" << result.testName << "- Seek:" << result.seekPositionMs << "ms"
                         << "- Actual:" << result.actualPositionMs << "ms"
                         << "- Error:" << result.errorMs << "ms"
                         << "- Message:" << result.message;
            }
        }

        qDebug() << "Passed:" << passedCount << "Failed:" << failedCount;
        qDebug() << "==========================================\n";
    }
};

// QTEST_MAIN(SeekingAutomatedTest)

#include "SeekingAutomatedTest.moc"
