#pragma once

#include <QColor>
#include <QFont>
#include <QPen>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QWidget>

class FullscreenTipsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FullscreenTipsWidget(QWidget *parent = 0);
    ~FullscreenTipsWidget();

private slots:
    void onFullscreenTipsDisplayTimerTimeout();

public:
    bool IsActivate();
    void SetRenderInfo(bool showOrHide, QString info);
    void SetRenderInfoWithoutTimer(bool showOrHide, QString info);

protected:
    void paintEvent(QPaintEvent *ev) override;

private:
    QPen textPen;
    QFont textFont;
    QRect area_;
    QString info_;
    QTimer *mTipsDisplayTimer;
    bool mDisplayTipsFlag = false;
};