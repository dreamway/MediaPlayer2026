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
        // 保持 Logo 原始大小，不进行缩放
        // Logo 居中显示在黑色背景上
        logoLabel_->setPixmap(logoPixmap_);
        logoLabel_->resize(logoPixmap_.size());

        if (logger) {
            logger->info("SloganWidget::setLogoPixmap: Logo set at original size={}x{}, widget size={}x{}",
                        logoPixmap_.width(), logoPixmap_.height(),
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

    // Logo 保持原始大小，不需要重新缩放
    // QLabel 在布局中会自动居中
}