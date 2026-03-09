#include "FullscreenTipsWidget.h"
#include <QPainter>

FullscreenTipsWidget::FullscreenTipsWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(100, 80);
    textPen.setColor(Qt::blue);
    textFont.setPixelSize(20);
    area_ = QRect(0, 0, 100, 80);
    mTipsDisplayTimer = new QTimer(this);
    connect(mTipsDisplayTimer, SIGNAL(timeout()), this, SLOT(onFullscreenTipsDisplayTimerTimeout()), Qt::DirectConnection);
}

FullscreenTipsWidget::~FullscreenTipsWidget()
{
    if (mTipsDisplayTimer) {
        mTipsDisplayTimer->stop();
        delete mTipsDisplayTimer;
        mTipsDisplayTimer = nullptr;
    }
}

void FullscreenTipsWidget::paintEvent(QPaintEvent *event)
{
    if (mDisplayTipsFlag) {
        QPainter painter;
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(textPen);
        painter.setFont(textFont);
        painter.drawText(area_, Qt::AlignCenter, info_);
        painter.end();
    } else {
        QPainter painter;
        painter.begin(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(textPen);
        painter.setFont(textFont);
        painter.drawText(area_, Qt::AlignCenter, "");
        painter.end();
    }
}

void FullscreenTipsWidget::SetRenderInfo(bool showOrHide, QString info)
{
    info_ = info;
    mDisplayTipsFlag = showOrHide;
    mTipsDisplayTimer->start(1000);
    repaint();
}

void FullscreenTipsWidget::SetRenderInfoWithoutTimer(bool showOrHide, QString info)
{
    if (false == IsActivate())
    {
        mDisplayTipsFlag = showOrHide;
        info_ = info;
    }
}

// 若为Activate, 说明当前已经有其它信息在显示
bool FullscreenTipsWidget::IsActivate() {
    return mTipsDisplayTimer->isActive();
}

void FullscreenTipsWidget::onFullscreenTipsDisplayTimerTimeout()
{
    mDisplayTipsFlag = false;
    mTipsDisplayTimer->stop();
    repaint();
}
