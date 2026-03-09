#include "UpShowComboBox.h"
#include "MainWindow.h"

void UpShowComboBox::showPopup()
{
    bool oldAnimationEffects = qApp->isEffectEnabled(Qt::UI_AnimateCombo);
    qApp->setEffectEnabled(Qt::UI_AnimateCombo, false);
    QComboBox::showPopup();
    QWidget *popup = this->findChild<QFrame *>();

    //针对当前ui布局，计算QCombobox全局坐标
    int combobox_y = static_cast<MainWindow *>(this->parent()->parent()->parent()->parent())->y()
                     + static_cast<QWidget *>(this->parent()->parent()->parent())->y() + static_cast<QWidget *>(this->parent()->parent())->y()
                     + static_cast<QWidget *>(this->parent())->y() + this->y();
    if (popup->y() > combobox_y) {
        popup->move(popup->x(), popup->y() - this->height() - popup->height()); //x轴不变，y轴向上移动 list的高+combox的高

    } else {
        //popup->move(popup->x(),popup->y()-this->height()-popup->height());//x轴不变，y轴向上移动 list的高+combox的高
    }

    qApp->setEffectEnabled(Qt::UI_AnimateCombo, oldAnimationEffects);
}