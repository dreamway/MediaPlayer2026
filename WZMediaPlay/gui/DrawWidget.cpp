#include "DrawWidget.h"
#include "spdlog/spdlog.h"
#include "ui_DrawWidget.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include "GlobalDef.h"



DrawWidget::DrawWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , ui(new Ui::DrawWidget)
{
    ui->setupUi(this);
    //startQpt = QPoint(0, 0);
    //endQpt = QPoint(0, 0);
    //drawing_ = false;

    /////////////////////////////////////////////////

    this->setMouseTracking(true);
    //  initStretchRect
    m_stretchRectState = StretchRectState::NotSelect;
    m_topLeftRect = QRect(0, 0, 0, 0);
    m_topRightRect = QRect(0, 0, 0, 0);
    m_bottomLeftRect = QRect(0, 0, 0, 0);
    m_bottomRightRect = QRect(0, 0, 0, 0);

    m_leftCenterRect = QRect(0, 0, 0, 0);
    m_topCenterRect = QRect(0, 0, 0, 0);
    m_rightCenterRect = QRect(0, 0, 0, 0);
    m_bottomCenterRect = QRect(0, 0, 0, 0);

    m_beginPoint = QPoint(0, 0);
    m_endPoint = QPoint(0, 0);

    m_currentCaptureState = CaptureState::InitCapture;
    ui->pushButton_Cancel->hide();
    ui->pushButton_Check->hide();
}

DrawWidget::~DrawWidget()
{
    delete ui;
}
// 获取当前鼠标位于哪一个拖拽顶点;
StretchRectState DrawWidget::getStrethRectState(QPoint point)
{
    StretchRectState stretchRectState = StretchRectState::NotSelect;
    if (m_topLeftRect.contains(point)) {
        stretchRectState = StretchRectState::TopLeftRect;
    } else if (m_topCenterRect.contains(point)) {
        stretchRectState = StretchRectState::TopCenterRect;
    } else if (m_topRightRect.contains(point)) {
        stretchRectState = StretchRectState::TopRightRect;
    } else if (m_rightCenterRect.contains(point)) {
        stretchRectState = StretchRectState::RightCenterRect;
    } else if (m_bottomRightRect.contains(point)) {
        stretchRectState = StretchRectState::BottomRightRect;
    } else if (m_bottomCenterRect.contains(point)) {
        stretchRectState = StretchRectState::BottomCenterRect;
    } else if (m_bottomLeftRect.contains(point)) {
        stretchRectState = StretchRectState::BottomLeftRect;
    } else if (m_leftCenterRect.contains(point)) {
        stretchRectState = StretchRectState::LeftCenterRect;
    }

    return stretchRectState;
}
// 设置鼠标停在拖拽定点处的样式;
void DrawWidget::setStretchCursorStyle(StretchRectState stretchRectState)
{
    switch (stretchRectState) {
    case StretchRectState::NotSelect:
        setCursor(Qt::ArrowCursor);
        break;
    case StretchRectState::TopLeftRect:
    case StretchRectState::BottomRightRect:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case StretchRectState::TopRightRect:
    case StretchRectState::BottomLeftRect:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case StretchRectState::LeftCenterRect:
    case StretchRectState::RightCenterRect:
        setCursor(Qt::SizeHorCursor);
        break;
    case StretchRectState::TopCenterRect:
    case StretchRectState::BottomCenterRect:
        setCursor(Qt::SizeVerCursor);
        break;
    default:
        break;
    }
}
// 当前鼠标坐标是否在选取的矩形区域内;
bool DrawWidget::isPressPointInSelectRect(QPoint mousePressPoint)
{
    QRect selectRect = getRect(m_beginPoint, m_endPoint);
    if (selectRect.contains(mousePressPoint)) {
        return true;
    }

    return false;
}

// 根据beginPoint , endPoint 获取当前选中的矩形;
QRect DrawWidget::getRect(const QPoint &beginPoint, const QPoint &endPoint)
{
    int x, y, width, height;
    width = qAbs(beginPoint.x() - endPoint.x());
    height = qAbs(beginPoint.y() - endPoint.y());
    x = beginPoint.x() < endPoint.x() ? beginPoint.x() : endPoint.x();
    y = beginPoint.y() < endPoint.y() ? beginPoint.y() : endPoint.y();

    QRect selectedRect = QRect(x, y, width, height);
    // 避免宽或高为零时拷贝截图有误;
    // 可以看QQ截图，当选取截图宽或高为零时默认为2;
    if (selectedRect.width() == 0) {
        selectedRect.setWidth(1);
    }
    if (selectedRect.height() == 0) {
        selectedRect.setHeight(1);
    }

    return selectedRect;
}
// 获取拖拽后的矩形选中区域;
QRect DrawWidget::getStretchRect()
{
    QRect stretchRect;
    QRect currentRect = getRect(m_beginPoint, m_endPoint);

    m_endMovePoint.setX(m_endMovePoint.x() < 0 ? 0 : m_endMovePoint.x());
    m_endMovePoint.setX(m_endMovePoint.x() > this->width() ? this->width() : m_endMovePoint.x());
    m_endMovePoint.setY(m_endMovePoint.y() < 0 ? 0 : m_endMovePoint.y());
    m_endMovePoint.setY(m_endMovePoint.y() > this->height() ? this->height() : m_endMovePoint.y());

    switch (m_stretchRectState) {
    case NotSelect:
        stretchRect = getRect(m_beginPoint, m_endPoint);
        break;
    case TopLeftRect: {
        stretchRect = getRect(currentRect.bottomRight(), m_endMovePoint);
    } break;
    case TopRightRect: {
        QPoint beginPoint = QPoint(currentRect.topLeft().x(), m_endMovePoint.y());
        QPoint endPoint = QPoint(m_endMovePoint.x(), currentRect.bottomRight().y());
        stretchRect = getRect(beginPoint, endPoint);
    } break;
    case BottomLeftRect: {
        QPoint beginPoint = QPoint(m_endMovePoint.x(), currentRect.topLeft().y());
        QPoint endPoint = QPoint(currentRect.bottomRight().x(), m_endMovePoint.y());
        stretchRect = getRect(beginPoint, endPoint);
    } break;
    case BottomRightRect: {
        stretchRect = getRect(currentRect.topLeft(), m_endMovePoint);
    } break;
    case LeftCenterRect: {
        QPoint beginPoint = QPoint(m_endMovePoint.x(), currentRect.topLeft().y());
        stretchRect = getRect(beginPoint, currentRect.bottomRight());
    } break;
    case TopCenterRect: {
        QPoint beginPoint = QPoint(currentRect.topLeft().x(), m_endMovePoint.y());
        stretchRect = getRect(beginPoint, currentRect.bottomRight());
    } break;
    case RightCenterRect: {
        QPoint endPoint = QPoint(m_endMovePoint.x(), currentRect.bottomRight().y());
        stretchRect = getRect(currentRect.topLeft(), endPoint);
    } break;
    case BottomCenterRect: {
        QPoint endPoint = QPoint(currentRect.bottomRight().x(), m_endMovePoint.y());
        stretchRect = getRect(currentRect.topLeft(), endPoint);
    } break;
    default: {
        stretchRect = getRect(m_beginPoint, m_endPoint);
    } break;
    }

    // 拖动结束更新 m_beginPoint , m_endPoint;
    if (m_currentCaptureState == FinishMoveStretchRect) {
        m_beginPoint = stretchRect.topLeft();
        m_endPoint = stretchRect.bottomRight();
    }

    return stretchRect;
}
// 绘制选中矩形各拖拽点小矩形;
void DrawWidget::drawStretchRect()
{
    QColor color = QColor(0, 174, 255);
    QRect drRect;
    if (!m_currentSelectRect.isNull()){
        drRect = m_currentSelectRect;
    } else if (!getRegion().isNull()){
        drRect = getRegion();
    }

    // 四个角坐标;
    QPoint topLeft = drRect.topLeft();
    QPoint topRight = drRect.topRight();
    QPoint bottomLeft = drRect.bottomLeft();
    QPoint bottomRight = drRect.bottomRight();
    // 四条边中间点坐标;
    QPoint leftCenter = QPoint(topLeft.x(), (topLeft.y() + bottomLeft.y()) / 2);
    QPoint topCenter = QPoint((topLeft.x() + topRight.x()) / 2, topLeft.y());
    QPoint rightCenter = QPoint(topRight.x(), leftCenter.y());
    QPoint bottomCenter = QPoint(topCenter.x(), bottomLeft.y());

    int HALF_STRETCH_RECT_WIDTH = floor(STRETCH_RECT_WIDTH / 2);
    int HALF_STRETCH_RECT_HEIGHT = floor(STRETCH_RECT_HEIGHT / 2);

    m_topLeftRect = QRect(topLeft.x() - HALF_STRETCH_RECT_WIDTH, topLeft.y() - HALF_STRETCH_RECT_HEIGHT, STRETCH_RECT_WIDTH, STRETCH_RECT_HEIGHT);
    m_topRightRect = QRect(
        topRight.x() - HALF_STRETCH_RECT_WIDTH + SELECT_RECT_BORDER_WIDTH, topRight.y() - HALF_STRETCH_RECT_HEIGHT, STRETCH_RECT_WIDTH, STRETCH_RECT_HEIGHT);
    m_bottomLeftRect = QRect(
        bottomLeft.x() - HALF_STRETCH_RECT_WIDTH, bottomLeft.y() - HALF_STRETCH_RECT_HEIGHT + SELECT_RECT_BORDER_WIDTH, STRETCH_RECT_WIDTH, STRETCH_RECT_HEIGHT);
    m_bottomRightRect = QRect(
        bottomRight.x() - HALF_STRETCH_RECT_WIDTH + SELECT_RECT_BORDER_WIDTH,
        bottomRight.y() - HALF_STRETCH_RECT_HEIGHT + SELECT_RECT_BORDER_WIDTH,
        STRETCH_RECT_WIDTH,
        STRETCH_RECT_HEIGHT);

    m_leftCenterRect = QRect(leftCenter.x() - HALF_STRETCH_RECT_WIDTH, leftCenter.y() - HALF_STRETCH_RECT_HEIGHT, STRETCH_RECT_WIDTH, STRETCH_RECT_HEIGHT);
    m_topCenterRect = QRect(topCenter.x() - HALF_STRETCH_RECT_WIDTH, topCenter.y() - HALF_STRETCH_RECT_HEIGHT, STRETCH_RECT_WIDTH, STRETCH_RECT_HEIGHT);
    m_rightCenterRect = QRect(
        rightCenter.x() - HALF_STRETCH_RECT_WIDTH + SELECT_RECT_BORDER_WIDTH,
        rightCenter.y() - HALF_STRETCH_RECT_HEIGHT,
        STRETCH_RECT_WIDTH,
        STRETCH_RECT_HEIGHT);
    m_bottomCenterRect = QRect(
        bottomCenter.x() - HALF_STRETCH_RECT_WIDTH,
        bottomCenter.y() - HALF_STRETCH_RECT_HEIGHT + SELECT_RECT_BORDER_WIDTH,
        STRETCH_RECT_WIDTH,
        STRETCH_RECT_HEIGHT);

    m_painter.fillRect(m_topLeftRect, color);
    m_painter.fillRect(m_topRightRect, color);
    m_painter.fillRect(m_bottomLeftRect, color);
    m_painter.fillRect(m_bottomRightRect, color);
    m_painter.fillRect(m_leftCenterRect, color);
    m_painter.fillRect(m_topCenterRect, color);
    m_painter.fillRect(m_rightCenterRect, color);
    m_painter.fillRect(m_bottomCenterRect, color);
}

// 获取移动后,当前选中的矩形;
QRect DrawWidget::getMoveRect()
{
    // 通过getMovePoint方法先检查当前是否移动超出屏幕;
    QPoint movePoint = getMovePoint();
    QPoint beginPoint = m_beginPoint + movePoint;
    QPoint endPoint = m_endPoint + movePoint;
    // 结束移动选区时更新当前m_beginPoint , m_endPoint,防止下一次操作时截取的图片有问题;
    if (m_currentCaptureState == FinishMoveCaptureArea) {
        m_beginPoint = beginPoint;
        m_endPoint = endPoint;
        m_beginMovePoint = QPoint(0, 0);
        m_endMovePoint = QPoint(0, 0);
    }
    return getRect(beginPoint, endPoint);
}

QPoint DrawWidget::getMovePoint()
{
    QPoint movePoint = m_endMovePoint - m_beginMovePoint;
    QRect currentRect = getRect(m_beginPoint, m_endPoint);
    // 检查当前是否移动超出屏幕;

    //移动选区是否超出屏幕左边界;
    if (currentRect.topLeft().x() + movePoint.x() < 0) {
        movePoint.setX(0 - currentRect.topLeft().x());
    }
    //移动选区是否超出屏幕上边界;
    if (currentRect.topLeft().y() + movePoint.y() < 0) {
        movePoint.setY(0 - currentRect.topLeft().y());
    }
    //移动选区是否超出屏幕右边界;
    if (currentRect.bottomRight().x() + movePoint.x() > this->width()) {
        movePoint.setX(this->width() - currentRect.bottomRight().x());
    }
    //移动选区是否超出屏幕下边界;
    if (currentRect.bottomRight().y() + movePoint.y() > this->height()) {
        movePoint.setY(this->height() - currentRect.bottomRight().y());
    }

    return movePoint;
}

// 根据当前截取状态获取当前选中的截图区域;
QRect DrawWidget::getSelectRect()
{
    QRect selectRect(0, 0, 0, 0);
    if (m_currentCaptureState == BeginCaptureImage || m_currentCaptureState == FinishCaptureImage) {
        selectRect = getRect(m_beginPoint, m_endPoint);
    } else if (m_currentCaptureState == BeginMoveCaptureArea || m_currentCaptureState == FinishMoveCaptureArea) {
        selectRect = getMoveRect();
    } else if (m_currentCaptureState == BeginMoveStretchRect || m_currentCaptureState == FinishMoveStretchRect) {
        selectRect = getStretchRect();
    }

    if (m_currentCaptureState == FinishCaptureImage || m_currentCaptureState == FinishMoveCaptureArea || m_currentCaptureState == FinishMoveStretchRect) {
        m_currentCaptureState = FinishCapture;
    }

    return selectRect;
}

// 绘制当前选中的截图区域;
void DrawWidget::drawCaptureImage()
{
    //m_capturePixmap = m_loadPixmap.copy(m_currentSelectRect);
    //m_painter.drawPixmap(m_currentSelectRect.topLeft(), m_capturePixmap);
    m_painter.setPen(QPen(QColor(0, 180, 255), SELECT_RECT_BORDER_WIDTH)); //设置画笔;
    if (!m_currentSelectRect.isNull()) {
        m_painter.drawRect(m_currentSelectRect);
    } else if (!getRegion().isNull()) {
        m_painter.drawRect(getRegion());
    }

    drawStretchRect();
}

void DrawWidget::moveButton(QPoint bPoint){
    ui->pushButton_Cancel->show();
    ui->pushButton_Check->show();
    ui->pushButton_Cancel->move(bPoint);
    ui->pushButton_Check->move(QPoint(bPoint.x() + 30, bPoint.y()));
}

void DrawWidget::on_pushButton_Check_clicked(){
    emit stereoRegionUpdate(getRegion());
    this->hide();
}

void DrawWidget::on_pushButton_Cancel_clicked() {
    this->setMouseTracking(true);
    //  initStretchRect
    m_stretchRectState = StretchRectState::NotSelect;
    m_topLeftRect = QRect(0, 0, 0, 0);
    m_topRightRect = QRect(0, 0, 0, 0);
    m_bottomLeftRect = QRect(0, 0, 0, 0);
    m_bottomRightRect = QRect(0, 0, 0, 0);

    m_leftCenterRect = QRect(0, 0, 0, 0);
    m_topCenterRect = QRect(0, 0, 0, 0);
    m_rightCenterRect = QRect(0, 0, 0, 0);
    m_bottomCenterRect = QRect(0, 0, 0, 0);

    m_beginPoint = QPoint(0, 0);
    m_endPoint = QPoint(0, 0);

    m_currentCaptureState = CaptureState::InitCapture;
    update();
}

void DrawWidget::mousePressEvent(QMouseEvent *event)
{
    m_stretchRectState = getStrethRectState(event->pos());
    if (event->button() == Qt::LeftButton) {
        if (m_currentCaptureState == CaptureState::InitCapture) {
            m_currentCaptureState = CaptureState::BeginCaptureImage;
            m_beginPoint = event->pos();
        }
        // 是否在拉伸的小矩形中;
        else if (m_stretchRectState != NotSelect) {
            m_currentCaptureState = CaptureState::BeginMoveStretchRect;
            // 当前鼠标在拖动选中区顶点时,设置鼠标当前状态;
            setStretchCursorStyle(m_stretchRectState);
            m_beginMovePoint = event->pos();
        }
        // 是否在选中的矩形中;
        else if (isPressPointInSelectRect(event->pos())) {
            m_currentCaptureState = CaptureState::BeginMoveCaptureArea;
            m_beginMovePoint = event->pos();
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////
    //   old code start
    ////logger->debug(
    ////    "drawWidget pos:{},size:{}, rect:{}, parentPos:{}, parentSize:{}, parentRect:{}",
    ////    this->pos(),
    ////    this->size(),
    ////    this->rect(),
    ////    parentWidget()->pos(),
    ////    parentWidget()->size(),
    ////    parentWidget()->rect());
    //startQpt = QPoint(event->localPos().x(), event->localPos().y());
    //endQpt = startQpt;
    //topLeft = QPoint(std::min(endQpt.x(), startQpt.x()), std::min(endQpt.y(), startQpt.y()));
    //bottomRight = QPoint(std::max(startQpt.x(), endQpt.x()), std::max(startQpt.y(), endQpt.y()));
    //logger->debug("press point:{}", startQpt.x());
    //drawing_ = true;
    //update();
    //   old code end
    //////////////////////////////////////////////////////////////////////////////////////////////
}

void DrawWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_currentCaptureState == CaptureState::BeginCaptureImage) {
        m_endPoint = event->pos();
        QPoint topLeft = QPoint(std::min(m_endPoint.x(), m_beginPoint.x()), std::min(m_endPoint.y(), m_beginPoint.y()));
        QPoint bottomRight = QPoint(std::max(m_endPoint.x(), m_beginPoint.x()), std::max(m_beginPoint.y(), m_endPoint.y()));
        m_currentSelectRect = QRect(topLeft, bottomRight);
        update();
    } else if (m_currentCaptureState == CaptureState::BeginMoveCaptureArea) {
        m_endMovePoint = event->pos();
        update();
    } else if (m_currentCaptureState == CaptureState::BeginMoveStretchRect) {
        m_endMovePoint = event->pos();
        update();
        // 当前鼠标在拖动选中区顶点时,在鼠标未停止移动前，一直保持鼠标当前状态;
    }

    // 根据鼠标是否在选中区域内设置鼠标样式;
    StretchRectState stretchRectState = getStrethRectState(event->pos());
    if (stretchRectState != NotSelect) {
        setStretchCursorStyle(stretchRectState);
    } else if (isPressPointInSelectRect(event->pos())) {
        setCursor(Qt::SizeAllCursor);
    } else if (!isPressPointInSelectRect(event->pos()) && m_currentCaptureState != CaptureState::BeginMoveCaptureArea) {
        setCursor(Qt::ArrowCursor);
    }

    /////////////////////////////////////////////////////////////////////////////
    //   old code start
    ////logger->debug(
    ////    "drawWidget pos:{},size:{}, rect:{}, parentPos:{}, parentSize:{}, parentRect:{}",
    ////    this->pos(),
    ////    this->size(),
    ////    this->rect(),
    ////    parentWidget()->pos(),
    ////    parentWidget()->size(),
    ////    parentWidget()->rect());
    //endQpt = QPoint(event->localPos().x(), event->localPos().y());
    //topLeft = QPoint(std::min(endQpt.x(), startQpt.x()), std::min(endQpt.y(), startQpt.y()));
    //bottomRight = QPoint(std::max(startQpt.x(), endQpt.x()), std::max(startQpt.y(), endQpt.y()));
    //drawing_ = true;
    //update();
    //   old code end
    /////////////////////////////////////////////////////////////////////////////////////////////
}

void DrawWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_currentCaptureState == CaptureState::BeginCaptureImage) {
  
        m_endPoint = event->pos();
        QPoint topLeft = QPoint(std::min(m_endPoint.x(), m_beginPoint.x()), std::min(m_endPoint.y(), m_beginPoint.y()));
        QPoint bottomRight = QPoint(std::max(m_endPoint.x(), m_beginPoint.x()), std::max(m_beginPoint.y(), m_endPoint.y()));
        m_beginPoint = topLeft;
        m_endPoint = bottomRight;
        m_currentSelectRect = QRect(topLeft, bottomRight);
        m_currentCaptureState = CaptureState::FinishCaptureImage;
        // 在选择完之后显示 确认或取消 按钮 （TODO：按钮跟着框选 走)
        ui->pushButton_Cancel->show();
        ui->pushButton_Check->show();
        update();
    } else if (m_currentCaptureState == CaptureState::BeginMoveCaptureArea) {
        m_currentCaptureState = CaptureState::FinishMoveCaptureArea;
        m_endMovePoint = event->pos();
        update();
    } else if (m_currentCaptureState == CaptureState::BeginMoveStretchRect) {
        m_currentCaptureState = CaptureState::FinishMoveStretchRect;
        m_endMovePoint = event->pos();
        update();
    }

    /////////////////////////////////////////////////////////////////////////////////
    //   old code start
    ////logger->debug(
    ////    "drawWidget pos:{},size:{}, rect:{}, parentPos:{}, parentSize:{}, parentRect:{}",
    ////    this->pos(),
    ////    this->size(),
    ////    this->rect(),
    ////    parentWidget()->pos(),
    ////    parentWidget()->size(),
    ////    parentWidget()->rect());

    //endQpt = QPoint(event->localPos().x(), event->localPos().y());

    //topLeft = QPoint(std::min(endQpt.x(), startQpt.x()), std::min(endQpt.y(), startQpt.y()));
    //bottomRight = QPoint(std::max(startQpt.x(), endQpt.x()), std::max(startQpt.y(), endQpt.y()));
    //drawing_ = false;
    //logger->info("mouseRelease, topLeft:{}x{}, bottomRight: {}x{}", topLeft.x(),topLeft.y(),bottomRight.x(), bottomRight.y());
    //update();
    ////emit stereoRegionUpdate(getRegion());
    //  old code end
    ////////////////////////////////////////////////////////////////////////////////////////////////////////
}

void DrawWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

#define VISUAL_DEBUG
void DrawWidget::paintEvent(QPaintEvent *event)
{
    m_painter.begin(this); //进行重绘;

    QColor shadowColor = QColor(0, 0, 0, 80);            //阴影颜色设置;
    m_painter.fillRect(this->rect(), shadowColor); //画影罩效果;

    switch (m_currentCaptureState) {
    case InitCapture:
        moveButton(QPoint(0, 0));
        ui->pushButton_Cancel->hide();
        ui->pushButton_Check->hide();
        break;
    case CaptureState::BeginCaptureImage:
    case CaptureState::FinishCaptureImage:
    case CaptureState::BeginMoveCaptureArea:
    case CaptureState::FinishMoveCaptureArea:
    case CaptureState::BeginMoveStretchRect:
    case CaptureState::FinishMoveStretchRect:
    case CaptureState::FinishCapture:
        m_currentSelectRect = getSelectRect();
        drawCaptureImage();
        {
            QPoint butPoint = QPoint(0,0);  //this->width() - 57, this->height() - 28
            if (!m_currentSelectRect.isNull()){
                butPoint = QPoint(m_currentSelectRect.bottomRight().x()-57, m_currentSelectRect.bottomRight().y()+2);
            } else if (!getRegion().isNull()) {
                butPoint = QPoint(getRegion().bottomRight().x() - 57, getRegion().bottomRight().y() + 2);
            }

            butPoint.setX(butPoint.x() < 0 ? 0 : butPoint.x());
            butPoint.setY(butPoint.y() > (this->height() - 28) ? (this->height() - 28) : butPoint.y());
            moveButton(butPoint);
        }
        break;
    default:
        break;
    }

    m_painter.end(); //重绘结束;


    /////////////////////////////////////////////////////////////////////////////////////
    //  old  code start
//    logger->debug("enter paintEvent");
//    if (false == drawing_)
//        return;
//
//    QPainter p;
//    p.begin(this);
//    //p.eraseRect(0, 0, width(), height());
//    QPen pen;
//    pen.setWidth(20);
//    pen.setColor(QColor(255, 0, 0));
//    pen.setStyle(Qt::DashDotLine);
//
//    QBrush brush;
//    brush.setColor(QColor(0, 255, 0));
//    brush.setStyle(Qt::Dense6Pattern);
//    p.setBrush(brush);
//
//    p.drawRect(startQpt.x(), startQpt.y(), (endQpt.x() - startQpt.x()), (endQpt.y() - startQpt.y()));
//#ifdef VISUAL_DEBUG
//    QPen debugPen;
//    debugPen.setWidth(10);
//    debugPen.setColor(QColor(255, 0, 0));
//    debugPen.setStyle(Qt::SolidLine);
//    p.setPen(debugPen);
//    p.drawPoint(topLeft);
//    debugPen.setColor(QColor(255, 0, 255));
//    p.setPen(debugPen);
//    p.drawPoint(bottomRight);
//#endif
//    p.end();
    //  old code end
    ///////////////////////////////////////////////////////////////////////////////////////////
}

QRect DrawWidget::getRegion()
{
    //return QRect(topLeft, bottomRight);
    return QRect(m_beginPoint, m_endPoint);
}

void DrawWidget::show() {
    QWidget::show();
    if (m_currentCaptureState == CaptureState::FinishCapture) {
        m_currentCaptureState = CaptureState::FinishMoveCaptureArea;
    }
    update();
}