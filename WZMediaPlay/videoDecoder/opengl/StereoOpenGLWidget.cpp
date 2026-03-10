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

#include "StereoOpenGLWidget.hpp"

#include <QOpenGLContext>
#include <QThread>
#include <QCoreApplication>

StereoOpenGLWidget::StereoOpenGLWidget()
{
    m_widget = this;
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(update()));
}
StereoOpenGLWidget::~StereoOpenGLWidget()
{
    makeCurrent();
}

bool StereoOpenGLWidget::makeContextCurrent()
{
    if (!context())
        return false;

    makeCurrent();
    return true;
}
void StereoOpenGLWidget::doneContextCurrent()
{
    doneCurrent();
}

void StereoOpenGLWidget::setVSync(bool enable)
{
    Q_UNUSED(enable)
    // Not supported
}
void StereoOpenGLWidget::updateGL(bool requestDelayed)
{
    // 检查 widget 是否有效和可见（避免在销毁过程中调用）
    if (!this || !isVisible()) {
        static int invisibleCount = 0;
        if (invisibleCount < 5) {  // 只输出前5次，避免日志过多
            if (logger) {
                logger->warn("StereoOpenGLWidget::updateGL: Widget not visible (isVisible={}, this={})", isVisible(), (void*)this);
            }
            invisibleCount++;
        }
        return;
    }

    // 检查 OpenGL 上下文是否有效（避免在上下文销毁后调用）
    if (!context() || !context()->isValid()) {
        static int invalidContextCount = 0;
        if (invalidContextCount < 5) {
            if (logger) {
                logger->warn("StereoOpenGLWidget::updateGL: OpenGL context invalid");
            }
            invalidContextCount++;
        }
        return;
    }

    // 如果在主线程中，直接调用update
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        update();
        return;
    }

    // 如果从非主线程调用，使用阻塞式连接确保同步渲染
    // 这样可以保证 VideoThread 等到渲染完成后再继续，避免数据竞争
    if (!requestDelayed) {
        QMetaObject::invokeMethod(this, "update", Qt::BlockingQueuedConnection);
    } else {
        QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
    }
}

void StereoOpenGLWidget::initializeGL()
{
    connect(context(), SIGNAL(aboutToBeDestroyed()), this, SLOT(aboutToBeDestroyed()), Qt::DirectConnection);
    StereoOpenGLCommon::initializeGL();
    initializeStereoShader();  // 初始化 3D Shader
}
void StereoOpenGLWidget::resizeGL(int w, int h)
{
    // 设置 viewport（QOpenGLWidget 会自动调用，但这里确保设置正确）
    glViewport(0, 0, w, h);
    StereoOpenGLCommon::newSize(false);  // 通知尺寸变化
}

void StereoOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);
    StereoOpenGLCommon::paintGL();  // 使用 StereoOpenGLCommon 的 paintGL（支持 3D）
}

void StereoOpenGLWidget::aboutToBeDestroyed()
{
    makeCurrent();
    contextAboutToBeDestroyed();
    doneCurrent();
}

bool StereoOpenGLWidget::event(QEvent *e)
{
    dispatchEvent(e, parent());
    return QOpenGLWidget::event(e);
}
