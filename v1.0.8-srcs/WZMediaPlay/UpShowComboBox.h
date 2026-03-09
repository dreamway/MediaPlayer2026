#pragma once
#include <QComboBox>

/**
自定义ComboBox,用于选择不同的输入格式
*/
class UpShowComboBox : public QComboBox
{
public:
    UpShowComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        //view()->installEventFilter(this);
    }

protected:
    void showPopup() override;
};
