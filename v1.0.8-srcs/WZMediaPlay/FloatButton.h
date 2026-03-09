#pragma once
#include <QWidget>

namespace Ui {
class FloatButton;
}

/**
关联播放器右侧播放列表的控件
*/
class FloatButton : public QWidget
{
    Q_OBJECT
public:
    explicit FloatButton(QWidget *parent = 0);
    ~FloatButton();
    void setShortcut(QKeySequence ks);
signals:
    void signals_playListShow_clicked();
private slots:
    void on_pushButton_clicked();

private:
    Ui::FloatButton *ui;
};
