#pragma once
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QWidget>

namespace Ui {
class AdvertisementWidget;
};
class AdvertisementWidget : public QWidget
{
public:
    explicit AdvertisementWidget(QWidget *parent = 0);
    ~AdvertisementWidget();

    void init(bool bImageOrVideo, QString playRes);

protected:
    virtual void resizeEvent(QResizeEvent *event);
    void paintEvent(QPaintEvent *event) override;
private slots:
    void updateImage();

private:
    Ui::AdvertisementWidget *ui;

    bool bPlayImageOrVideo = false; // true=image, false=video；析构前必须由 init() 设置
    QMediaPlayer *mQMediaPlayer = nullptr;
    QAudioOutput *mQAudioOutput = nullptr;

    QTimer *mQTimer = nullptr;  // init 中创建 mQTimer 的代码被注释时未初始化，析构需判空
    QStringList mQStringList;
    int index = 0;
};
