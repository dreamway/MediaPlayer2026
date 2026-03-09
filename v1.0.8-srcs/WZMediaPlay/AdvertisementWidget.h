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

    bool bPlayImageOrVideo; //  true imgage    false  video
    QMediaPlayer *mQMediaPlayer;
    QAudioOutput *mQAudioOutput;

    QTimer *mQTimer;
    QStringList mQStringList;
    int index = 0;
};
