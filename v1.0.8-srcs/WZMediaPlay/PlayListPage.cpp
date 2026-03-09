#include "PlayListPage.h"
#include "MainWindow.h"
#include "ui_PlayListPage.h"
#include <iostream>

PlayListPage::PlayListPage(MainWindow *mw, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PlayListPage)
{
    ui->setupUi(this);
    mMainWindow = mw;
    connect(ui->listWidget_playlist, &DropListWidget::signalDropEvent, this, &PlayListPage::reply_listWidget_playlist_dropEvent);
}

PlayListPage::~PlayListPage() {}

QListWidget *PlayListPage::getListWidget()
{
    return ui->listWidget_playlist;
}

void PlayListPage::on_listWidget_playlist_itemClicked(QListWidgetItem *item)
{
    mMainWindow->on_listWidget_playlist_itemClicked(item);
}

void PlayListPage::on_listWidget_playlist_itemDoubleClicked(QListWidgetItem *item)
{
    mMainWindow->on_listWidget_playlist_itemDoubleClicked(item);
}

void PlayListPage::reply_listWidget_playlist_itemSelectionChanged(QListWidgetItem *listWidgetItem)
{
    mMainWindow->reply_listWidget_playlist_itemSelectionChanged(listWidgetItem);
}

void PlayListPage::reply_listWidget_playlist_dropEvent(int index)
{
    mMainWindow->replayListWidgetDrop(index);
}