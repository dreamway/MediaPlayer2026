#pragma once

#include "3rdparty/srtparser.h"
#include <QColor>
#include <QFont>
#include <QPen>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <qtemporaryfile.h>

class SubtitleWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SubtitleWidget(QWidget *parent = 0);
    ~SubtitleWidget();

private slots:
    void onSubtitleDisplayTimerTimeout();

public:
    bool SetSubtitleFile(QString &subtitleFilename);
    bool Seek(int64_t timestamp);

    bool PlayPause(bool playFlag);
    bool Start();
    bool Stop();
    bool IsStarted();

protected:
    void paintEvent(QPaintEvent *ev) override;

private:
    QString mSubtitleFilename;
    std::vector<SubtitleItem *> mSubtitleItems;
    int mCurSubtitleItemIndex = 0;
    long mElapsedSinceStart = 0;

    QPen textPen;
    QFont textFont;

    std::string mCurSubtitleText;
    QTimer *mSubtitleDisplayTimer;
    int mSubtitleCheckingInterval = 300;
    bool mDisplaySubtitleFlag = false;
    bool mPauseFlag = false;

    int mFontAreaHeight = 100;
    bool mIsEnabled = false;

private:
    long mCurBeginTime = 0;
    long mCurEndTime = 0;

private:
    SubtitleParserFactory *subParserFactory = nullptr;
    SubtitleParser *parser = nullptr;
    
    // Windows平台临时文件，用于处理中文路径问题
#ifdef _WIN32
    QTemporaryFile *tempSubtitleFile_ = nullptr;
#endif
};