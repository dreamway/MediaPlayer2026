#pragma once
#include <QListWidget>
#include <QWidget>

namespace Ui {
class PlayListPage;
};
class MainWindow;
class PlayListPage : public QWidget
{
    Q_OBJECT
public:
    explicit PlayListPage(MainWindow *mw, QWidget *parent = nullptr);
    ~PlayListPage();
    QListWidget *getListWidget();
signals:
    void itemClicked(QListWidgetItem *item);
    void itemDoubleClicked(QListWidgetItem *item);
    void itemSelectionChanged(QListWidgetItem *listWidgetItem);
private slots:
    void on_listWidget_playlist_itemClicked(QListWidgetItem *item);
    void on_listWidget_playlist_itemDoubleClicked(QListWidgetItem *item);
    void reply_listWidget_playlist_itemSelectionChanged(QListWidgetItem *listWidgetItem);
    void reply_listWidget_playlist_dropEvent(int index);

private:
    Ui::PlayListPage *ui;
    MainWindow *mMainWindow;
};
