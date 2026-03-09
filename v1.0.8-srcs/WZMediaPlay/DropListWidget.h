#pragma once
#include <QListWidget>
#include <QDropEvent>

class DropListWidget:public QListWidget
{
	Q_OBJECT
public:
    explicit DropListWidget(QWidget *parent = Q_NULLPTR);
    ~DropListWidget();
    void dropEvent(QDropEvent *event);
    //void dragEnterEvent(QDragEnterEvent *e);

signals:
    void signalDropEvent(int index);
private:
};
