/*
    SloganWidget: Logo 显示控件实现
*/

#include "SloganWidget.h"
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

SloganWidget::SloganWidget(QWidget *parent)
    : QWidget(parent)
{
    initializeUI();
}

void SloganWidget::initializeUI()
{
    // 设置黑色背景
    // 注意：必须设置 WA_StyledBackground 属性，否则样式表不生效
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: black;");

    // 创建主布局
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setSpacing(0);

    // 创建 Logo 标签
    logoLabel_ = new QLabel(this);
    logoLabel_->setAlignment(Qt::AlignCenter);
    logoLabel_->setStyleSheet("background-color: transparent;");
    logoLabel_->setScaledContents(false);

    // 添加到布局
    mainLayout_->addWidget(logoLabel_, 0, Qt::AlignCenter);

    if (logger) {
        logger->info("SloganWidget: Initialized with black background");
    }
}

void SloganWidget::setLogoPixmap(const QPixmap &pixmap)
{
    logoPixmap_ = pixmap;

    if (logoLabel_ && !logoPixmap_.isNull()) {
        // 根据窗口大小缩放 Logo，保持宽高比
        QSize labelSize = size();

        // 如果窗口大小为 0，使用默认大小
        if (labelSize.isEmpty()) {
            labelSize = QSize(400, 300);
        }

        QSize scaledSize = logoPixmap_.size().scaled(labelSize, Qt::KeepAspectRatio);

        // 确保 Logo 不会太大
        if (scaledSize.isEmpty()) {
            scaledSize = logoPixmap_.size();
        }

        QPixmap scaledPixmap = logoPixmap_.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        logoLabel_->setPixmap(scaledPixmap);
        logoLabel_->resize(scaledSize);

        if (logger) {
            logger->info("SloganWidget::setLogoPixmap: Logo set, original size={}x{}, scaled size={}x{}, widget size={}x{}",
                        logoPixmap_.width(), logoPixmap_.height(),
                        scaledSize.width(), scaledSize.height(),
                        width(), height());
        }
    } else {
        if (logger) {
            logger->warn("SloganWidget::setLogoPixmap: Failed to set logo, logoLabel_={}, pixmap.isNull()={}",
                        (void*)logoLabel_, pixmap.isNull());
        }
    }
}

void SloganWidget::clearLogo()
{
    if (logoLabel_) {
        logoLabel_->clear();
    }
    logoPixmap_ = QPixmap();

    if (logger) {
        logger->debug("SloganWidget::clearLogo: Logo cleared");
    }
}

void SloganWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (logger) {
        logger->info("SloganWidget::resizeEvent: new size={}x{}", width(), height());
    }

    // 重新缩放 Logo 以适应新窗口大小
    if (logoLabel_ && !logoPixmap_.isNull()) {
        QSize labelSize = size();
        QSize scaledSize = logoPixmap_.size().scaled(labelSize, Qt::KeepAspectRatio);

        QPixmap scaledPixmap = logoPixmap_.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        logoLabel_->setPixmap(scaledPixmap);
        logoLabel_->resize(scaledSize);

        if (logger) {
            logger->info("SloganWidget::resizeEvent: Logo rescaled to {}x{}", scaledSize.width(), scaledSize.height());
        }
    } else {
        if (logger) {
            logger->warn("SloganWidget::resizeEvent: No logo to rescale, logoLabel_={}, pixmap.isNull()={}",
                        (void*)logoLabel_, logoPixmap_.isNull());
        }
    }
}