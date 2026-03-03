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

#include "StereoOpenGLWindow.hpp"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QDockWidget>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>

StereoOpenGLWindow::StereoOpenGLWindow()
    : m_platformName(QCoreApplication::instance() ? QGuiApplication::platformName() : QString())
    , m_passEventsToParent(m_platformName != "xcb" && m_platformName != "android")
    , m_visible(true)  // 初始化为 true，假设窗口默认可见
{
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(doUpdateGL()));

    if (!m_passEventsToParent)
        setFlags(Qt::WindowTransparentForInput);

    m_widget = QWidget::createWindowContainer(this);
    if (!m_platformName.contains("wayland") && !m_platformName.contains("android"))
        m_widget->setAttribute(Qt::WA_NativeWindow);
    m_widget->installEventFilter(this);
    m_widget->setAcceptDrops(false);
    
    // 初始化时设置可见性
    if (m_widget) {
        m_visible = m_widget->isVisible();
    }
}

StereoOpenGLWindow::~StereoOpenGLWindow()
{
    makeCurrent();
}

void StereoOpenGLWindow::deleteMe()
{
    delete m_widget;
}

bool StereoOpenGLWindow::makeContextCurrent()
{
    if (!context())
        return false;

    makeCurrent();
    return true;
}
void StereoOpenGLWindow::doneContextCurrent()
{
    doneCurrent();
}

void StereoOpenGLWindow::setVSync(bool enable)
{
    QSurfaceFormat fmt = format();
    if (!handle())
    {
        fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
        fmt.setSwapInterval(enable);
        setFormat(fmt);
    }
    else if (enable != (fmt.swapInterval() != 0))
    {
        fmt.setSwapInterval(enable);
        destroy();
        setFormat(fmt);
        create();
        setVisible(true);
    }
    vSync = enable;
}
void StereoOpenGLWindow::updateGL(bool requestDelayed)
{
    // 修复：初始时 m_visible 可能为 false，但窗口可能已经可见
    // 如果窗口可见或已暴露，就允许更新
    if (isExposed() || m_widget->isVisible())
    {
        if (!m_visible && m_widget->isVisible())
        {
            m_visible = true;  // 更新可见性标志
        }
        QMetaObject::invokeMethod(this, "doUpdateGL", Qt::QueuedConnection, Q_ARG(bool, requestDelayed));
    }
}

void StereoOpenGLWindow::initializeGL()
{
    connect(context(), SIGNAL(aboutToBeDestroyed()), this, SLOT(aboutToBeDestroyed()), Qt::DirectConnection);
    StereoOpenGLCommon::initializeGL();
    initializeStereoShader();  // 初始化 3D Shader
}
void StereoOpenGLWindow::resizeGL(int w, int h)
{
    // 设置 viewport
    glViewport(0, 0, w, h);
    StereoOpenGLCommon::newSize(false);  // 通知尺寸变化
}

void StereoOpenGLWindow::paintGL()
{
    if (isExposed())
    {
        glClear(GL_COLOR_BUFFER_BIT);
        StereoOpenGLCommon::paintGL();  // 使用 StereoOpenGLCommon 的 paintGL（支持 3D）
    }
}

void StereoOpenGLWindow::doUpdateGL(bool queued)
{
    if (queued)
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest), Qt::LowEventPriority);
    else
    {
        QEvent updateEvent(QEvent::UpdateRequest);
        QCoreApplication::sendEvent(this, &updateEvent);
    }
}
void StereoOpenGLWindow::aboutToBeDestroyed()
{
    makeCurrent();
    contextAboutToBeDestroyed();
    doneCurrent();
}
void StereoOpenGLWindow::videoVisible(bool v)
{
    m_visible = v && (m_widget->visibleRegion() != QRegion());
}

bool StereoOpenGLWindow::eventFilter(QObject *o, QEvent *e)
{
    if (o == m_widget)
    {
        if (e->type() == QEvent::Resize)
        {
            QResizeEvent *re = dynamic_cast<QResizeEvent *>(e);
            if (re && re->size().width() > 0 && re->size().height() > 0)
            {
                resize(re->size());
                videoVisible(m_widget->isVisible());
            }
        }
        else if (e->type() == QEvent::Show || e->type() == QEvent::Hide)
        {
            videoVisible(m_widget->isVisible());
        }
    }
    return QOpenGLWindow::eventFilter(o, e);
}

bool StereoOpenGLWindow::event(QEvent *e)
{
    dispatchEvent(e, parent());
    return QOpenGLWindow::event(e);
}
