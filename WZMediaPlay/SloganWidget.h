/*
    SloganWidget: Logo 显示控件
    独立的 Widget，用于在未播放视频时显示 Logo
    与 StereoVideoWidget 和 CameraOpenGLWidget 同级
*/

#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPixmap>

/**
 * SloganWidget: Logo 显示控件
 * 职责：
 * - 提供黑色背景的 Logo 显示区域
 * - 与 StereoVideoWidget、CameraOpenGLWidget 同级管理
 * - 简化 Logo 显示/隐藏逻辑，避免 QOpenGLWidget 子控件的 z-order 问题
 */
class SloganWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SloganWidget(QWidget *parent = nullptr);
    ~SloganWidget() override = default;

    /**
     * 设置 Logo 图片
     * @param pixmap Logo 图片
     */
    void setLogoPixmap(const QPixmap &pixmap);

    /**
     * 清除 Logo 图片
     */
    void clearLogo();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void initializeUI();

private:
    QLabel *logoLabel_ = nullptr;      // Logo 标签
    QVBoxLayout *mainLayout_ = nullptr; // 主布局
    QPixmap logoPixmap_;                // Logo 图片缓存
};