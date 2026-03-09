#pragma once
#include <QMainWindow>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionTab>

class CustomTabStyle : public QProxyStyle
{
    Q_OBJECT
public:
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
};
