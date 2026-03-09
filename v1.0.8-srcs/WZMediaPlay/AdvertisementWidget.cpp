#include "AdvertisementWidget.h"
#include "ui_AdvertisementWidget.h"
#include <QDir>
#include <QPainter>
#include <QTimer>

AdvertisementWidget::AdvertisementWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AdvertisementWidget)
{
    ui->setupUi(this);
}

void AdvertisementWidget::init(bool bImageOrVideo, QString playRes)
{
    bPlayImageOrVideo = bImageOrVideo;
    if (bPlayImageOrVideo) {
        //ui->videoWidget_advertisement->hide();
        //QDir imgDir;
        //imgDir.setPath(playRes);
        //QStringList ImageList;
        //ImageList << "*.bmp"
        //          << "*.jpg"
        //          << "*.png";
        //imgDir.setNameFilters(ImageList);
        //for (int i = 0; i < imgDir.count(); ++i) {
        //    mQStringList.append(QString(playRes + "/" + imgDir[i]));
        //}

        //mQTimer = new QTimer(this);
        //connect(mQTimer, &QTimer::timeout, this, &AdvertisementWidget::updateImage);

        //QTimer::singleShot(5000, [&] { mQTimer->start(2000); });
        //----------------------------------------------------------------------------
        mQStringList.clear();
        mQStringList.append(playRes);

        ui->videoWidget_advertisement->hide();
        QImage image;
        image.load(playRes);
    } else {
        mQMediaPlayer = new QMediaPlayer();
        mQAudioOutput = new QAudioOutput();

        mQMediaPlayer->setPlaybackRate(1.0);
        mQAudioOutput->setVolume(0);

        ui->videoWidget_advertisement->setAspectRatioMode(Qt::KeepAspectRatio);

        mQMediaPlayer->setVideoOutput(ui->videoWidget_advertisement);
        mQMediaPlayer->setAudioOutput(mQAudioOutput);

        mQMediaPlayer->setSource(QUrl::fromLocalFile(playRes)); //  "E:/workspace/Windows/dreamwaycopy/wxFFmpeg/Release/Sample.mp4"
        mQMediaPlayer->setLoops(-1);

        QTimer::singleShot(5000, [&] {
            mQMediaPlayer->play();
            QSize video_size = ui->videoWidget_advertisement->sizeHint();
            int v_h = this->width() * video_size.height() / video_size.width();
            int v_y = 0;
            if (v_h > this->height()) {
                //int v_w = this->height() * video_size.width() / video_size.height();
                //ui->videoWidget_advertisement->resize(this->width(), this->height());
            } else {
                //ui->videoWidget_advertisement->resize(this->width(), v_h);
                v_y = (this->height() - v_h) / 2;
            }
            //ui->videoWidget_advertisement->move(0, v_y);
        });
    }
}

AdvertisementWidget::~AdvertisementWidget()
{
    if (bPlayImageOrVideo) {
        mQTimer->stop();
    } else {
        mQMediaPlayer->stop();
    }
    delete ui;
}

void AdvertisementWidget::updateImage()
{
    if (index <= (mQStringList.size() - 1)) {
        QImage image;
        image.load(mQStringList[index]);

        //double scaled_width = image.width() * 1.0 / this->width();
        //double scaled_height = image.height() * 1.0 / this->height();
        //if (scaled_width > scaled_height) {
        //    double scale = this->width() * 1.0 / image.width();
        //    image = image.scaled(this->width(), scale * image.height());
        //} else {
        //    double scale = this->height() * 1.0 / image.height();
        //    image = image.scaled(this->height(), scale * image.width());
        //}
        ////ui->label_advertisement->setPixmap(QPixmap::fromImage(image));
        //index = (index + 1) > (mQStringList.size() - 1) ? 0 : (index + 1);
    }
}

void AdvertisementWidget::resizeEvent(QResizeEvent *event) {}

void AdvertisementWidget::paintEvent(QPaintEvent *event)
{
    if (bPlayImageOrVideo && mQStringList.size() > 0) {
        QPainter p(this);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        QImage image;
        image.load(mQStringList[0]);
        image = image.scaled(this->rect().size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        int x = this->rect().width() / 2 - image.size().width() / 2;
        int y = this->rect().height() / 2 - image.size().height() / 2;

        p.fillRect(this->rect(), Qt::white);
        p.drawImage(QPoint(x, y), image);
    }
}