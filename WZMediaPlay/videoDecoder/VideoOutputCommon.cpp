/*
    QMPlay2 is a video and audio player.
    Copyright (C) 2010-2025  Błażej Szczygieł

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "VideoOutputCommon.hpp"
#include <QCoreApplication>
#include <QWidget>
#include <QMouseEvent>
#include <QEvent>

VideoOutputCommon::VideoOutputCommon(bool yInverted)
    : m_yMultiplier(yInverted ? -1.0f : 1.0f)
{
}

VideoOutputCommon::~VideoOutputCommon()
{
}

QWidget *VideoOutputCommon::widget() const
{
    return m_widget;
}

void VideoOutputCommon::resetOffsets()
{
    m_videoOffset = QPointF();
    m_osdOffset = QPointF();
}



QSize VideoOutputCommon::getRealWidgetSize() const
{
    return m_widget ? m_widget->size() : QSize();
}

void VideoOutputCommon::updateSizes(bool transpose)
{
    // TODO: 完整实现
}

void VideoOutputCommon::updateMatrix()
{
    // TODO: 完整实现
}

void VideoOutputCommon::dispatchEvent(QEvent *e, QObject *p)
{
    // 参考 QMPlayer2 的实现：只处理特定事件，避免无限递归
    if (!e) {
        return;
    }
    
    switch (e->type())
    {
        case QEvent::MouseButtonPress:
            if (m_sphericalView)
                mousePress360(static_cast<QMouseEvent *>(e));
            else
                mousePress(static_cast<QMouseEvent *>(e));
            break;
        case QEvent::MouseButtonRelease:
            if (m_sphericalView)
                mouseRelease360(static_cast<QMouseEvent *>(e));
            else
                mouseRelease(static_cast<QMouseEvent *>(e));
            break;
        case QEvent::MouseMove:
            if (m_sphericalView)
                mouseMove360(static_cast<QMouseEvent *>(e));
            else
                mouseMove(static_cast<QMouseEvent *>(e));
            break;
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
            m_canWrapMouse = false;
            // fallthrough
        case QEvent::TouchEnd:
        case QEvent::Gesture:
            // 只对 Touch 和 Gesture 事件转发给父对象
            if (p) {
                QCoreApplication::sendEvent(p, e);
            }
            break;
        default:
            // 其他事件不处理，避免递归
            break;
    }
}

void VideoOutputCommon::mousePress(QMouseEvent *e)
{
    // TODO: 完整实现
}

void VideoOutputCommon::mouseMove(QMouseEvent *e)
{
    // TODO: 完整实现
}

void VideoOutputCommon::mouseRelease(QMouseEvent *e)
{
    // TODO: 完整实现
}

void VideoOutputCommon::mousePress360(QMouseEvent *e)
{
    // TODO: 完整实现
}

void VideoOutputCommon::mouseMove360(QMouseEvent *e)
{
    // TODO: 完整实现
}

void VideoOutputCommon::mouseRelease360(QMouseEvent *e)
{
    // TODO: 完整实现
}

void VideoOutputCommon::rotValueUpdated(const QVariant &value)
{
    // TODO: 完整实现
}
