#pragma once
#include <QMouseEvent>
#include <QSlider>

/**
定制化的PlayProgress, 便于UI跳转
*/
class CustomSlider : public QSlider
{
    Q_OBJECT
public:
    CustomSlider(QWidget *parent = 0)
        : QSlider(parent)
    {}

protected:
    void mousePressEvent(QMouseEvent *ev); //重写QSlider的mousePressEvent事件
signals:
    void customSliderClicked(int seekTarget); //自定义的鼠标单击信号，用于捕获并处理
};
