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

#include "OpenGLWidget.hpp"

#include <QOpenGLContext>
#include <QThread>
#include <QCoreApplication>

OpenGLWidget::OpenGLWidget()
{
    m_widget = this;
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(update()));
}
OpenGLWidget::~OpenGLWidget()
{
    makeCurrent();
}

bool OpenGLWidget::makeContextCurrent()
{
    if (!context())
        return false;

    makeCurrent();
    return true;
}
void OpenGLWidget::doneContextCurrent()
{
    doneCurrent();
}

void OpenGLWidget::setVSync(bool enable)
{
    Q_UNUSED(enable)
    // Not supported
}
void OpenGLWidget::updateGL(bool requestDelayed)
{
    // 检查 widget 是否有效和可见（避免在销毁过程中调用）
    if (!this || !isVisible()) {
        return;
    }

    // 检查 OpenGL 上下文是否有效（避免在上下文销毁后调用）
    if (!context() || !context()->isValid()) {
        return;
    }

    // 如果在主线程中，直接调用update
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        update();
        return;
    }

    // 优化：总是使用非阻塞式连接，避免VideoThread被阻塞
    // 帧数据通过引用计数保护，确保渲染时数据有效
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void OpenGLWidget::initializeGL()
{
    connect(context(), SIGNAL(aboutToBeDestroyed()), this, SLOT(aboutToBeDestroyed()), Qt::DirectConnection);
    OpenGLCommon::initializeGL();
}
void OpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    OpenGLCommon::paintGL();
}

void OpenGLWidget::aboutToBeDestroyed()
{
    makeCurrent();
    contextAboutToBeDestroyed();
    doneCurrent();
}

bool OpenGLWidget::event(QEvent *e)
{
    dispatchEvent(e, parent());
    return QOpenGLWidget::event(e);
}
