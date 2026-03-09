#include "AboutDialog.h"
#include "GlobalDef.h"
#include "ui_AboutDialog.h"
#include <iostream>
#include <QFile>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint);
    QString urlStr = QString("%1/doc/MediaPlayLicense.pdf").arg(QCoreApplication::applicationDirPath());
    //QString copyRightInfo =
    //	QString(tr("维真公司版权所有<br>"))
    //	+ QString(tr("Copyright © 2024 - 2026[WeiZheng]<br>"))
    //	+ QString(tr("宁波维真科技有限公司，保留所有权利<br>"))
    //	+ QString("<a style = 'color: blue; text-decoration: none' href = " + urlStr + tr(">License"));

    QString aboutCopyRight;
    if (GlobalDef::getInstance()->LANGUAGE == 0) {
        aboutCopyRight = GlobalDef::getInstance()->ABOUT_CR_ZHCN;
    } else if (GlobalDef::getInstance()->LANGUAGE == 1) {
        aboutCopyRight = GlobalDef::getInstance()->ABOUT_CR_EN;
    } else if (GlobalDef::getInstance()->LANGUAGE == 2) {
        aboutCopyRight = GlobalDef::getInstance()->ABOUT_CR_ZHHANT;
    }

    QString copyRightInfo = aboutCopyRight + QString("<a style = 'color: blue; text-decoration: none' href = " + urlStr + tr(">License"));

    ui->label_company->setText(QString(tr("3D MediaPlayer")));
    ui->label_version->setText("V1.0.8, build 2025-12-30");
    ui->label_copyright->setOpenExternalLinks(true);
    ui->label_copyright->setText(copyRightInfo);

    QFile file(QString("%1/doc/EULA.txt").arg(QCoreApplication::applicationDirPath())); //创建文件对象
    if (file.open(QIODevice::ReadOnly))                                                 //只读方式打开
    {
        QString content = file.readAll();              //读取内容
        ui->textEdit_copyright->setPlainText(content); //显示内容到textEdit文本框
        file.close();                                  //关闭文件
    }
}

AboutDialog::~AboutDialog() {}