#include "DropListWidget.h"
#include <QDebug>

DropListWidget::DropListWidget(QWidget *parent):QListWidget(parent){
    this->setAcceptDrops(true);
    this->setDragEnabled(true);
}

DropListWidget ::~DropListWidget(){}

void DropListWidget::dropEvent(QDropEvent *event){
    emit signalDropEvent(indexAt(event->position().toPoint()).row());
}
