#pragma once
#include<QWidget>
#include <QPainter>

namespace Ui {
class DrawWidget;
};

typedef enum { 
	NotSelect = 1000, 

	TopLeftRect = 1001, 
	TopCenterRect = 1002,
    TopRightRect =1003,

	LeftCenterRect = 1004,
	RightCenterRect = 1005,

	BottomLeftRect = 1006,
    BottomCenterRect = 1007,
    BottomRightRect = 1008
} StretchRectState;

typedef enum { 
	InitCapture = 2000,

	BeginCaptureImage=2001,
    BeginMoveStretchRect = 2002,
    BeginMoveCaptureArea = 2003,

    FinishCaptureImage = 2004,
    FinishMoveCaptureArea = 2005,
    FinishMoveStretchRect = 2006,

    FinishCapture =2007

} CaptureState;

class DrawWidget:public QWidget
{
    Q_OBJECT
public:
    explicit DrawWidget(QWidget *parent = 0, Qt::WindowFlags f = Qt::WindowFlags());
    ~DrawWidget();

public:
	QRect getRegion();
public Q_SLOTS:
    void show();

protected:
	void mousePressEvent(QMouseEvent* event);
	void mouseMoveEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent* event);
	void paintEvent(QPaintEvent* event);
	void resizeEvent(QResizeEvent* event);
private:
    StretchRectState getStrethRectState(QPoint point);
    void setStretchCursorStyle(StretchRectState stretchRectState);
    bool isPressPointInSelectRect(QPoint mousePressPoint);
    QRect getRect(const QPoint &beginPoint, const QPoint &endPoint);
    QRect getStretchRect();
    void drawStretchRect();
    QRect getMoveRect();
    QPoint getMovePoint();
    QRect getSelectRect();
    void drawCaptureImage();
    void moveButton(QPoint bPoint);
private slots:
    void on_pushButton_Check_clicked();
    void on_pushButton_Cancel_clicked();
signals:
    void stereoRegionUpdate(QRect region);

private:
	Ui::DrawWidget* ui;

	//  选中矩形框顶点矩形
    int STRETCH_RECT_WIDTH = 5;
    int STRETCH_RECT_HEIGHT = 5;
    int SELECT_RECT_BORDER_WIDTH = 1;
    QRect m_topLeftRect;
    QRect m_topCenterRect;
    QRect m_topRightRect;
	QRect m_leftCenterRect;
    QRect m_rightCenterRect;
	QRect m_bottomLeftRect;
    QRect m_bottomCenterRect;
    QRect m_bottomRightRect;

	StretchRectState m_stretchRectState;
    CaptureState m_currentCaptureState;
	//  选中矩形框 左上、右下点
    QPoint m_beginPoint;
    QPoint m_endPoint;
    QRect m_currentSelectRect;

	QPoint m_beginMovePoint, m_endMovePoint;

	QPainter m_painter;
};
