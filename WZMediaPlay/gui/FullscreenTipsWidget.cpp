#include "FullscreenTipsWidget.h"
#include <QPainter>

FullscreenTipsWidget::FullscreenTipsWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(120, 100);
    // BUG-050 修复：设置背景色和文字颜色，确保提示可见
    // 使用半透明黑色背景，白色文字
    //setStyleSheet("background-color: rgba(0, 0, 0, 180);");
    textPen.setColor(Qt::blue);
    textFont.setPixelSize(20);
    //textFont.setBold(true);
    area_ = QRect(0, 0, 120, 100);
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
    if (showOrHide) {
        show();
        raise();
    }
    mTipsDisplayTimer->start(1000);
    repaint();
}

void FullscreenTipsWidget::SetRenderInfoWithoutTimer(bool showOrHide, QString info)
{
    if (false == IsActivate())
    {
        mDisplayTipsFlag = showOrHide;
        info_ = info;
        if (showOrHide) {
            show();
            raise();
        } else {
            hide();
        }
        repaint();
    }
}

// 
bool FullscreenTipsWidget::IsActivate() {
    return mTipsDisplayTimer->isActive();
}

void FullscreenTipsWidget::onFullscreenTipsDisplayTimerTimeout()
{
    mDisplayTipsFlag = false;
    mTipsDisplayTimer->stop();
    hide();
    repaint();
}
