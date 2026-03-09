#include "FloatButton.h"
#include "ui_FloatButton.h"

FloatButton::FloatButton(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FloatButton)
{
    ui->setupUi(this);
}

FloatButton::~FloatButton()
{
    delete ui;
}

void FloatButton::setShortcut(QKeySequence ks)
{
    ui->pushButton->setShortcut(ks);
}

void FloatButton::on_pushButton_clicked()
{
    emit signals_playListShow_clicked();
}