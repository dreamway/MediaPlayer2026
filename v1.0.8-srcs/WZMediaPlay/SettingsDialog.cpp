#include "SettingsDialog.h"
#include "ApplicationSettings.h"
#include "CustomTabStyle.h"
#include "GlobalDef.h"
#include "spdlog/spdlog.h"
#include "ui_SettingsDialog.h"
#include <QDebug>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QTranslator>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString(tr("设置")));
    ui->tabWidget->setTabPosition(QTabWidget::West);
    ui->tabWidget->tabBar()->setStyle(new CustomTabStyle);

    ui->lineEdit_splash_logo->setText(GlobalDef::getInstance()->SPLASH_LOGO_PATH);
    ui->lineEdit_play_window_logo->setText(GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH);
    ui->lineEdit_menu_icon_logo->setText(GlobalDef::getInstance()->MAIN_WINDOW_LOGO_PATH);

    if (GlobalDef::getInstance()->mRunningMode == RunningMode::RM_RELEASE) {
        ui->label_splash_logo_title->hide();
        ui->lineEdit_splash_logo->hide();
        ui->pushButton_splash_logo->hide();

        ui->label_play_window_logo_title->hide();
        ui->lineEdit_play_window_logo->hide();
        ui->pushButton_play_window_logo->hide();

        ui->label_menu_icon_title->hide();
        ui->lineEdit_menu_icon_logo->hide();
        ui->pushButton_menu_icon_logo->hide();
    }

    {
        ui->label_height_title->hide();
        ui->lineEdit_height_left->hide();
        ui->lineEdit_height_3d->hide();
        ui->lineEdit_height_right->hide();
    }

    //  language settings
    ui->comboBox_language->addItem(QString(tr("简体中文")));
    ui->comboBox_language->addItem(QString(tr("英语")));
    ui->comboBox_language->addItem(QString(tr("繁体中文")));
    ui->comboBox_language->setCurrentIndex(GlobalDef::getInstance()->LANGUAGE);

    //  move window size settings
    moveWindowSizeButtonGroup = new QButtonGroup(this);
    moveWindowSizeButtonGroup->setExclusive(true);
    moveWindowSizeButtonGroup->addButton(ui->radioButton_move_window_size_mfw, 0);
    moveWindowSizeButtonGroup->addButton(ui->radioButton_move_windows_size_fs, 1);
    moveWindowSizeButtonGroup->addButton(ui->radioButton_move_window_size_wfm, 2);
    MOVE_FIT_WINDOW_SATAE_TEMP = GlobalDef::getInstance()->MOVE_FIT_WINDOW_SATAE;
    switch (GlobalDef::getInstance()->MOVE_FIT_WINDOW_SATAE) {
    case 0:
        ui->radioButton_move_window_size_mfw->setChecked(true);
        break;
    case 1:
        ui->radioButton_move_windows_size_fs->setChecked(true);
        break;
    case 2:
        ui->radioButton_move_window_size_wfm->setChecked(true);
        break;
    }
    connect(moveWindowSizeButtonGroup, &QButtonGroup::idToggled, this, &SettingsDialog::reply_moveWindowSizeButtonGroup_idToggled);

    //  screenshot settings
    ui->lineEdit_screenshot_save_path->setText(GlobalDef::getInstance()->SCREENSHOT_DIR);

    //  subtitle settings
    ui->checkBox_auto_load_same_name->setChecked(GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME);

    //  audio output settings
    ui->comboBox_voice_select->addItem(QString(tr("输出到默认输出设备")));

    //  init hotKey
    ApplicationSettings appSettings;
    appSettings.read_Hotkey();
    ui->keySequenceEdit_hotkey_file_openfile->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile"));
    ui->keySequenceEdit_hotkey_file_closefile->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_CloseFile"));
    ui->keySequenceEdit_hotkey_file_previous->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Previous"));
    ui->keySequenceEdit_hotkey_file_next->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Next"));

    ui->keySequenceEdit_hotkey_play_playpause->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Pause"));
    ui->keySequenceEdit_hotkey_play_2D3D->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D"));
    ui->keySequenceEdit_hotkey_play_LR->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR"));
    ui->keySequenceEdit_hotkey_play_RL->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL"));
    ui->keySequenceEdit_hotkey_play_UD->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD"));
    ui->keySequenceEdit_hotkey_play_vertical->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical"));
    ui->keySequenceEdit_hotkey_play_horizontal->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal"));
    ui->keySequenceEdit_hotkey_play_chess->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess"));
    ui->keySequenceEdit_hotkey_play_region->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region"));

    //ui->keySequenceEdit_hotkey_image_fullscreen->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Fullscreen);
    ui->keySequenceEdit_hotkey_image_screenshot->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot"));

    ui->keySequenceEdit_hotkey_voice_volup->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volup"));
    ui->keySequenceEdit_hotkey_voice_volde->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volde"));
    ui->keySequenceEdit_hotkey_voice_mute->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Mute"));

    ui->keySequenceEdit_hotkey_subtitle_loadsubtitle->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle"));
    ui->keySequenceEdit_hotkey_subtitle_changesubtitle->setKeySequence(
        GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle"));

    ui->keySequenceEdit_hotkey_others_playlist->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_PlayList"));
    ui->keySequenceEdit_hotkey_others_fullscreen_plus->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus"));
    ui->keySequenceEdit_hotkey_others_fullscreen->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen"));
    ui->keySequenceEdit_hotkey_others_increase_parallax->setKeySequence(
        GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax"));
    ui->keySequenceEdit_hotkey_others_decrease_parallax->setKeySequence(
        GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax"));
    ui->keySequenceEdit_hotkey_others_reset_parallax->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax"));

    advTypeLeftButtonGroup = new QButtonGroup(this);
    advTypeLeftButtonGroup->setExclusive(true);
    advTypeLeftButtonGroup->addButton(ui->radioButton_dvertisement_null_left, 0);
    advTypeLeftButtonGroup->addButton(ui->radioButton_dvertisement_image_left, 1);
    advTypeLeftButtonGroup->addButton(ui->radioButton_dvertisement_video_left, 2);
    switch ((int) GlobalDef::getInstance()->ADV_TYPE_LEFT) {
    case 0:
        ui->radioButton_dvertisement_null_left->setChecked(true);
        break;
    case 1:
        ui->radioButton_dvertisement_image_left->setChecked(true);
        break;
    case 2:
        ui->radioButton_dvertisement_video_left->setChecked(true);
        break;
    }
    connect(advTypeLeftButtonGroup, &QButtonGroup::idToggled, this, &SettingsDialog::reply_advLeftButtonGroup_idToggled);

    ui->lineEdit_source_path_left->setText(GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT);

    advTypeRightButtonGroup = new QButtonGroup(this);
    advTypeRightButtonGroup->setExclusive(true);
    advTypeRightButtonGroup->addButton(ui->radioButton_dvertisement_null_right, 0);
    advTypeRightButtonGroup->addButton(ui->radioButton_dvertisement_image_right, 1);
    advTypeRightButtonGroup->addButton(ui->radioButton_dvertisement_video_right, 2);
    switch ((int) GlobalDef::getInstance()->ADV_TYPE_RIGHT) {
    case 0:
        ui->radioButton_dvertisement_null_right->setChecked(true);
        break;
    case 1:
        ui->radioButton_dvertisement_image_right->setChecked(true);
        break;
    case 2:
        ui->radioButton_dvertisement_video_right->setChecked(true);
        break;
    }
    connect(advTypeRightButtonGroup, &QButtonGroup::idToggled, this, &SettingsDialog::reply_advRightButtonGroup_idToggled);

    ui->lineEdit_source_path_right->setText(GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT);

    ui->lineEdit_width_left->setValidator(new QIntValidator(0, 999999, this));
    ui->lineEdit_height_left->setValidator(new QIntValidator(0, 999999, this));
    ui->lineEdit_width_3d->setValidator(new QIntValidator(0, 999999, this));
    ui->lineEdit_height_3d->setValidator(new QIntValidator(0, 999999, this));
    ui->lineEdit_width_right->setValidator(new QIntValidator(0, 999999, this));
    ui->lineEdit_height_right->setValidator(new QIntValidator(0, 999999, this));

    ui->lineEdit_width_left->setText(QString::number(GlobalDef::getInstance()->ADV_WIDTH_LEFT));
    ui->lineEdit_height_left->setText(QString::number(GlobalDef::getInstance()->ADV_HEIGHT_LEFT));
    ui->lineEdit_width_3d->setText(QString::number(GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH));
    ui->lineEdit_height_3d->setText(QString::number(GlobalDef::getInstance()->PLAY_3D_VIEW_HEIGHT));
    ui->lineEdit_width_right->setText(QString::number(GlobalDef::getInstance()->ADV_WIDTH_RIGHT));
    ui->lineEdit_height_right->setText(QString::number(GlobalDef::getInstance()->ADV_WIDTH_RIGHT));
}

SettingsDialog::~SettingsDialog() {}

bool SettingsDialog::checkHotKey(QString item, QString editKey)
{
    QList<QString> keyList = GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.keys(); // 存放的就是QMap的key值
    qDebug() << "============>keyList:" << keyList;
    qDebug() << "------------>editKey:" << editKey;
    if (editKey == QString("Esc"))
        return false;
    for (int i = 0; i < keyList.size(); i++) {
        if (keyList[i] != item) {
            if (GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value(keyList[i]) == editKey)
                return false;
        }
    }

    return true;
}

void SettingsDialog::on_comboBox_language_activated(int index)
{
    //TODO:
    //qDebug() << "================================= on_comboBox_language_currentIndexChanged index:" << index;
    //static QTranslator* translator;
    //if (translator != NULL)
    //{
    //	qApp->removeTranslator(translator);
    //	delete translator;
    //	translator = NULL;
    //}
    //translator = new QTranslator;
    //QString languagePath = QCoreApplication::applicationDirPath();
    //switch (index) {
    //case 0:
    //	languagePath = languagePath + QString("/Resources/language/language_zh_CN.qm");
    //	break;
    //case 1:
    //	languagePath = languagePath + QString("/Resources/language/language_en.qm");
    //	break;
    //case 2:
    //	languagePath = languagePath + QString("/Resources/language/language_zh_Hant.qm");
    //	break;
    //}
    //if (translator->load(languagePath)) {
    //	qApp->installTranslator(translator);
    //}
    //ui->retranslateUi(this);

    //GlobalDef::getInstance()->LANGUAGE = index;
    //ApplicationSettings appSettings;
    //appSettings.write_ApplicationGeneral();
}
void SettingsDialog::on_pushButton_splash_logo_clicked()
{
    QString chosePath = ui->lineEdit_splash_logo->text();
    QString fileName = QFileDialog::getOpenFileName(this, QString(tr("请选择启动logo文件")), chosePath, "images(*.png)");
    if (!fileName.isEmpty()) {
        ui->lineEdit_splash_logo->setText(fileName);
        //ApplicationSettings appSettings;
        //appSettings.write_SplashLogoPath(fileName);
        //GlobalDef::getInstance()->SPLASH_LOGO_PATH = fileName;
        //QMessageBox::information(this, QString(tr("信息")),
        //	QString(tr("启动logo已更新")),
        //	QMessageBox::NoButton, QMessageBox::Close);
    }
}
void SettingsDialog::on_pushButton_play_window_logo_clicked()
{
    QString chosePath = ui->lineEdit_play_window_logo->text();

    QString fileName = QFileDialog::getOpenFileName(this, QString(tr("请选择播放logo文件")), chosePath, "images(*.png)");
    if (!fileName.isEmpty()) {
        ui->lineEdit_play_window_logo->setText(fileName);
    }
}
void SettingsDialog::on_pushButton_menu_icon_logo_clicked()
{
    QString chosePath = ui->lineEdit_menu_icon_logo->text();

    QString fileName = QFileDialog::getOpenFileName(this, QString(tr("请选择主窗logo文件")), chosePath, "images(*.png)");
    if (!fileName.isEmpty()) {
        ui->lineEdit_menu_icon_logo->setText(fileName);
    }
}

void SettingsDialog::on_pushButton_system_setting_confirm_clicked()
{
    {
        int index = ui->comboBox_language->currentIndex();
        static QTranslator *translator;
        if (translator != NULL) {
            qApp->removeTranslator(translator);
            delete translator;
            translator = NULL;
        }
        translator = new QTranslator;
        QString languagePath = QCoreApplication::applicationDirPath();
        switch (index) {
        case 0:
            languagePath = languagePath + QString("/Resources/language/language_zh_CN.qm");
            break;
        case 1:
            languagePath = languagePath + QString("/Resources/language/language_en.qm");
            break;
        case 2:
            languagePath = languagePath + QString("/Resources/language/language_zh_Hant.qm");
            break;
        }
        if (translator->load(languagePath)) {
            qApp->installTranslator(translator);
        }
        ui->retranslateUi(this);

        GlobalDef::getInstance()->LANGUAGE = index;
        emit updateLanguage();
    }
    GlobalDef::getInstance()->SPLASH_LOGO_PATH = ui->lineEdit_splash_logo->text();
    GlobalDef::getInstance()->PLAY_WINDOW_LOGO_PATH = ui->lineEdit_play_window_logo->text();
    GlobalDef::getInstance()->MAIN_WINDOW_LOGO_PATH = ui->lineEdit_menu_icon_logo->text();
    ApplicationSettings mWZSetting;
    mWZSetting.write_SplashLogoPath(ui->lineEdit_splash_logo->text());
    mWZSetting.write_ApplicationGeneral();
    //QMessageBox::information(this, QString(tr("信息")),
    //	QString(tr("logo已更新，软件重启后生效")),
    //	QMessageBox::NoButton, QMessageBox::Close);
    close();
}
void SettingsDialog::on_pushButton_system_setting_cancel_clicked()
{
    close();
}

void SettingsDialog::reply_moveWindowSizeButtonGroup_idToggled(int id, bool checked)
{
    if (checked) {
        MOVE_FIT_WINDOW_SATAE_TEMP = id;
    }
}
void SettingsDialog::on_pushButton_play_setting_confirm_clicked()
{
    GlobalDef::getInstance()->MOVE_FIT_WINDOW_SATAE = MOVE_FIT_WINDOW_SATAE_TEMP;
    ApplicationSettings appSettings;
    appSettings.write_ApplicationGeneral();
    close();
}
void SettingsDialog::on_pushButton_play_setting_cancel_clicked()
{
    close();
}

void SettingsDialog::on_pushButton_screenshot_select_path_clicked()
{
    QString defSaveDir = ui->lineEdit_screenshot_save_path->text();
    QString saveDir = QFileDialog::getExistingDirectory(this, QString(tr("选择截图保存路径")), defSaveDir);

    if (!saveDir.isEmpty()) {
        ui->lineEdit_screenshot_save_path->setText(saveDir);
    }
}
void SettingsDialog::on_pushButton_img_setting_confirm_clicked()
{
    GlobalDef::getInstance()->SCREENSHOT_DIR = ui->lineEdit_screenshot_save_path->text();
    QMessageBox::information(this, QString(tr("信息")), QString(tr("保存图像路径已更新")), QMessageBox::NoButton, QMessageBox::Close);
    ApplicationSettings appSettings;
    appSettings.write_ApplicationGeneral();
    close();
}
void SettingsDialog::on_pushButton_img_setting_cancel_clicked()
{
    close();
}

void SettingsDialog::on_checkBox_auto_load_same_name_stateChanged(int state)
{
    if (state == Qt::Unchecked || state == Qt::Checked) {
        GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME = state;
        ApplicationSettings appSettings;
        appSettings.write_ApplicationGeneral();
    }
}

//void SettingsDialog::on_checkBox_auto_load_same_dir_stateChanged(int state)
//{
//    //if (state == Qt::Unchecked || state == Qt::Checked) {
//    //	GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR = state;
//    //	ApplicationSettings appSettings;
//    //	appSettings.write_ApplicationGeneral();
//    //}
//}

void SettingsDialog::on_pushButton_subtitle_select_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, QString(tr("请选择字幕文件")), GlobalDef::getInstance()->CURRENT_MOVIE_PATH);
    if (!fileName.isEmpty()) {
        ui->lineEdit_subtitle_filename->setText(fileName);
    }
}

void SettingsDialog::on_pushButton_subtitle_setting_confirm_clicked()
{
    bool loadSameName = ui->checkBox_auto_load_same_name->isChecked();
    QString userSelectedSubtitleFn = ui->lineEdit_subtitle_filename->text();
    logger->info(
        "on_pushButton_subtitle_setting_confirm_clicked, loadSameNameFlag:{}, userSelectedSubtitleFn:{}", loadSameName, userSelectedSubtitleFn.toStdString());

    if (static_cast<int>(loadSameName) != GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME
        || GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME != userSelectedSubtitleFn) {
        logger->info(
            "load samedir_samename:{}, path:{}",
            GlobalDef::getInstance()->SUBTITLE_AUTO_LOAD_SAMEDIR_SAMENAME,
            GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.toStdString());
        ApplicationSettings appSettings;
        appSettings.write_ApplicationGeneral();
        GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME = userSelectedSubtitleFn;

        //若在播放过程中选择，则此时应该在Player中加载字幕
        emit subtitleSettingsChanged();
        close();
    } else {
        logger->info(
            "loadSameName != GlobalDef || Global SubtitleFilename:{} != userSelectedSubtitleFn:{} ",
            GlobalDef::getInstance()->USER_SELECTED_SUBTITLE_FILENAME.toUtf8().constData(),
            userSelectedSubtitleFn.toUtf8().constData());
        close();
    }
}

void SettingsDialog::on_pushButton_subtitle_setting_cancel_clicked()
{
    close();
}

void SettingsDialog::on_comboBox_voice_select_activated(int index)
{
    //TODO
    logger->info("  activated:{}", index);
}

void SettingsDialog::on_pushButton_voice_setting_confirm_clicked()
{
    close();
}
void SettingsDialog::on_pushButton_voice_setting_cancel_clicked()
{
    close();
}

void SettingsDialog::on_pushButton_hotkey_file_restore_default_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_OpenFile"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_OpenFile").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_CloseFile"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_CloseFile").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Previous"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_Previous").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Next"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("FileTab_Next").toString());

    ui->keySequenceEdit_hotkey_file_openfile->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_OpenFile"));
    ui->keySequenceEdit_hotkey_file_closefile->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_CloseFile"));
    ui->keySequenceEdit_hotkey_file_previous->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Previous"));
    ui->keySequenceEdit_hotkey_file_next->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("FileTab_Next"));

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
}
void SettingsDialog::on_pushButton_hotkey_file_cancel_clicked()
{
    close();
}
void SettingsDialog::on_pushButton_hotkey_file_confirm_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_OpenFile"] = ui->keySequenceEdit_hotkey_file_openfile->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_CloseFile"] = ui->keySequenceEdit_hotkey_file_closefile->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Previous"] = ui->keySequenceEdit_hotkey_file_previous->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["FileTab_Next"] = ui->keySequenceEdit_hotkey_file_next->keySequence();

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
    close();
}

void SettingsDialog::on_pushButton_hotkey_play_restore_default_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Pause"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Pause").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_2D3D"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_2D3D").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_LR"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_LR").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_RL"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_RL").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_UD"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_UD").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Vertical"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Vertical").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Horizontal"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Horizontal").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Chess"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Chess").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Region"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("PlayTab_Region").toString());

    ui->keySequenceEdit_hotkey_play_playpause->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Pause"));
    ui->keySequenceEdit_hotkey_play_2D3D->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_2D3D"));
    ui->keySequenceEdit_hotkey_play_LR->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_LR"));
    ui->keySequenceEdit_hotkey_play_RL->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_RL"));
    ui->keySequenceEdit_hotkey_play_UD->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_UD"));
    ui->keySequenceEdit_hotkey_play_vertical->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Vertical"));
    ui->keySequenceEdit_hotkey_play_horizontal->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Horizontal"));
    ui->keySequenceEdit_hotkey_play_chess->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Chess"));
    ui->keySequenceEdit_hotkey_play_region->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("PlayTab_Region"));

    ApplicationSettings appSettings;
    appSettings.write_Hotkey();

    emit hotKeyChanged();
}
void SettingsDialog::on_pushButton_hotkey_play_cancel_clicked()
{
    close();
}
void SettingsDialog::on_pushButton_hotkey_play_confirm_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Pause"] = ui->keySequenceEdit_hotkey_play_playpause->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_2D3D"] = ui->keySequenceEdit_hotkey_play_2D3D->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_LR"] = ui->keySequenceEdit_hotkey_play_LR->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_RL"] = ui->keySequenceEdit_hotkey_play_RL->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_UD"] = ui->keySequenceEdit_hotkey_play_UD->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Vertical"] = ui->keySequenceEdit_hotkey_play_vertical->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Horizontal"] = ui->keySequenceEdit_hotkey_play_horizontal->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Chess"] = ui->keySequenceEdit_hotkey_play_chess->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["PlayTab_Region"] = ui->keySequenceEdit_hotkey_play_region->keySequence();

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
    close();
}

void SettingsDialog::on_pushButton_hotkey_image_restore_default_clicked()
{
    //GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["ImageTab_Fullscreen"] = QKeySequence(GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("ImageTab_Fullscreen").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["ImageTab_Screenshot"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("ImageTab_Screenshot").toString());

    //ui->keySequenceEdit_hotkey_image_fullscreen->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.ImageTab_Fullscreen);
    ui->keySequenceEdit_hotkey_image_screenshot->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("ImageTab_Screenshot"));

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
}
void SettingsDialog::on_pushButton_hotkey_image_cancel_clicked()
{
    close();
}
void SettingsDialog::on_pushButton_hotkey_image_confirm_clicked()
{
    //GlobalDef::getInstance()->userWZKeySequence.ImageTab_Fullscreen = ui->keySequenceEdit_hotkey_image_fullscreen->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["ImageTab_Screenshot"] = ui->keySequenceEdit_hotkey_image_screenshot->keySequence();

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
    close();
}

void SettingsDialog::on_pushButton_hotkey_voice_restore_default_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volup"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("VoiceTab_Volup").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volde"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("VoiceTab_Volde").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Mute"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("VoiceTab_Mute").toString());

    ui->keySequenceEdit_hotkey_voice_volup->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volup"));
    ui->keySequenceEdit_hotkey_voice_volde->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volde"));
    ui->keySequenceEdit_hotkey_voice_mute->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Mute"));

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
}
void SettingsDialog::on_pushButton_hotkey_voice_cancel_clicked()
{
    close();
}
void SettingsDialog::on_pushButton_hotkey_voice_confirm_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volup"] = ui->keySequenceEdit_hotkey_voice_volup->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Volde"] = ui->keySequenceEdit_hotkey_voice_volde->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["VoiceTab_Mute"] = ui->keySequenceEdit_hotkey_voice_mute->keySequence();

    logger->info("VoiceTab_Volup:{}", GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("VoiceTab_Volup").toString().toStdString());

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
    close();
}

void SettingsDialog::on_pushButton_hotkey_subtitle_restore_default_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_LoadSubtitle"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_ChangeSubtitle"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle").toString());

    ui->keySequenceEdit_hotkey_subtitle_loadsubtitle->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_LoadSubtitle"));
    ui->keySequenceEdit_hotkey_subtitle_changesubtitle->setKeySequence(
        GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("SubtitleTab_ChangeSubtitle"));

    ApplicationSettings mWZSetting;
    mWZSetting.write_Hotkey();

    emit hotKeyChanged();
}
void SettingsDialog::on_pushButton_hotkey_subtitle_cancel_clicked()
{
    close();
}
void SettingsDialog::on_pushButton_hotkey_subtitle_confirm_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_LoadSubtitle"] = ui->keySequenceEdit_hotkey_subtitle_loadsubtitle->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["SubtitleTab_ChangeSubtitle"] = ui->keySequenceEdit_hotkey_subtitle_changesubtitle->keySequence();

    ApplicationSettings appSettings;
    appSettings.write_Hotkey();

    emit hotKeyChanged();
    close();
}

void SettingsDialog::on_pushButton_hotkey_others_restore_default_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_PlayList"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_PlayList").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreenPlus"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreen"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_FullScreen").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_IncreaseParallax"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_DecreaseParallax"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax").toString());
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_ResetParallax"] = QKeySequence(
        GlobalDef::getInstance()->defWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax").toString());

    ui->keySequenceEdit_hotkey_others_playlist->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_PlayList"));
    ui->keySequenceEdit_hotkey_others_fullscreen_plus->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreenPlus"));
    ui->keySequenceEdit_hotkey_others_fullscreen->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_FullScreen"));
    ui->keySequenceEdit_hotkey_others_increase_parallax->setKeySequence(
        GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_IncreaseParallax"));
    ui->keySequenceEdit_hotkey_others_decrease_parallax->setKeySequence(
        GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_DecreaseParallax"));
    ui->keySequenceEdit_hotkey_others_reset_parallax->setKeySequence(GlobalDef::getInstance()->userWZKeySequence.hotKeyMap.value("OthersTab_ResetParallax"));

    ApplicationSettings appSettings;
    appSettings.write_Hotkey();

    emit hotKeyChanged();
}
void SettingsDialog::on_pushButton_hotkey_others_cancel_clicked()
{
    //reject();
    close();
}
void SettingsDialog::on_pushButton_hotkey_others_confirm_clicked()
{
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_PlayList"] = ui->keySequenceEdit_hotkey_others_playlist->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreenPlus"] = ui->keySequenceEdit_hotkey_others_fullscreen_plus->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_FullScreen"] = ui->keySequenceEdit_hotkey_others_fullscreen->keySequence();

    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_IncreaseParallax"] = ui->keySequenceEdit_hotkey_others_increase_parallax->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_DecreaseParallax"] = ui->keySequenceEdit_hotkey_others_decrease_parallax->keySequence();
    GlobalDef::getInstance()->userWZKeySequence.hotKeyMap["OthersTab_ResetParallax"] = ui->keySequenceEdit_hotkey_others_reset_parallax->keySequence();

    ApplicationSettings appSettings;
    appSettings.write_Hotkey();

    emit hotKeyChanged();
    close();
}

void SettingsDialog::on_keySequenceEdit_hotkey_file_openfile_keySequenceChanged()
{
    logger->debug("hotKeySequenceChanged");
}

void SettingsDialog::on_keySequenceEdit_hotkey_file_openfile_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_file_openfile->keySequence().toString();
    if (!checkHotKey("FileTab_OpenFile", editValue)) {
        ui->keySequenceEdit_hotkey_file_openfile->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_file_closefile_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_file_closefile->keySequence().toString();
    if (!checkHotKey("FileTab_CloseFile", editValue)) {
        ui->keySequenceEdit_hotkey_file_closefile->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_file_previous_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_file_previous->keySequence().toString();
    if (!checkHotKey("FileTab_Previous", editValue)) {
        ui->keySequenceEdit_hotkey_file_previous->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_file_next_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_file_next->keySequence().toString();
    if (!checkHotKey("FileTab_Next", editValue)) {
        ui->keySequenceEdit_hotkey_file_next->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_playpause_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_playpause->keySequence().toString();
    if (!checkHotKey("PlayTab_Pause", editValue)) {
        ui->keySequenceEdit_hotkey_play_playpause->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_2D3D_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_2D3D->keySequence().toString();
    if (!checkHotKey("PlayTab_2D3D", editValue)) {
        ui->keySequenceEdit_hotkey_play_2D3D->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_LR_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_LR->keySequence().toString();
    if (!checkHotKey("PlayTab_LR", editValue)) {
        ui->keySequenceEdit_hotkey_play_LR->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_RL_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_RL->keySequence().toString();
    if (!checkHotKey("PlayTab_RL", editValue)) {
        ui->keySequenceEdit_hotkey_play_RL->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_UD_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_UD->keySequence().toString();
    if (!checkHotKey("PlayTab_UD", editValue)) {
        ui->keySequenceEdit_hotkey_play_UD->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_vertical_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_vertical->keySequence().toString();
    if (!checkHotKey("PlayTab_Vertical", editValue)) {
        ui->keySequenceEdit_hotkey_play_vertical->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_horizontal_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_horizontal->keySequence().toString();
    if (!checkHotKey("PlayTab_Horizontal", editValue)) {
        ui->keySequenceEdit_hotkey_play_horizontal->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_chess_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_chess->keySequence().toString();
    if (!checkHotKey("PlayTab_Chess", editValue)) {
        ui->keySequenceEdit_hotkey_play_chess->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_play_region_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_play_region->keySequence().toString();
    if (!checkHotKey("PlayTab_Region", editValue)) {
        ui->keySequenceEdit_hotkey_play_region->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_image_screenshot_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_image_screenshot->keySequence().toString();
    if (!checkHotKey("ImageTab_Screenshot", editValue)) {
        ui->keySequenceEdit_hotkey_image_screenshot->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_voice_volup_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_voice_volup->keySequence().toString();
    if (!checkHotKey("VoiceTab_Volup", editValue)) {
        ui->keySequenceEdit_hotkey_voice_volup->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_voice_volde_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_voice_volde->keySequence().toString();
    if (!checkHotKey("VoiceTab_Volde", editValue)) {
        ui->keySequenceEdit_hotkey_voice_volde->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_voice_mute_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_voice_mute->keySequence().toString();
    if (!checkHotKey("VoiceTab_Mute", editValue)) {
        ui->keySequenceEdit_hotkey_voice_mute->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_subtitle_loadsubtitle_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_subtitle_loadsubtitle->keySequence().toString();
    if (!checkHotKey("SubtitleTab_LoadSubtitle", editValue)) {
        ui->keySequenceEdit_hotkey_subtitle_loadsubtitle->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_subtitle_changesubtitle_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_subtitle_changesubtitle->keySequence().toString();
    if (!checkHotKey("SubtitleTab_ChangeSubtitle", editValue)) {
        ui->keySequenceEdit_hotkey_subtitle_changesubtitle->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_others_playlist_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_others_playlist->keySequence().toString();
    if (!checkHotKey("OthersTab_PlayList", editValue)) {
        ui->keySequenceEdit_hotkey_others_playlist->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_others_fullscreen_plus_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_others_fullscreen_plus->keySequence().toString();
    if (!checkHotKey("OthersTab_FullScreenPlus", editValue)) {
        ui->keySequenceEdit_hotkey_others_fullscreen_plus->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_others_fullscreen_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_others_fullscreen->keySequence().toString();
    if (!checkHotKey("OthersTab_FullScreen", editValue)) {
        ui->keySequenceEdit_hotkey_others_fullscreen->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_others_increase_parallax_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_others_increase_parallax->keySequence().toString();
    if (!checkHotKey("OthersTab_IncreaseParallax", editValue)) {
        ui->keySequenceEdit_hotkey_others_increase_parallax->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_others_decrease_parallax_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_others_decrease_parallax->keySequence().toString();
    if (!checkHotKey("OthersTab_DecreaseParallax", editValue)) {
        ui->keySequenceEdit_hotkey_others_decrease_parallax->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::on_keySequenceEdit_hotkey_others_reset_parallax_editingFinished()
{
    QString editValue = ui->keySequenceEdit_hotkey_others_reset_parallax->keySequence().toString();
    if (!checkHotKey("OthersTab_ResetParallax", editValue)) {
        ui->keySequenceEdit_hotkey_others_reset_parallax->clear();
        QMessageBox::warning(this, QString(tr("警告")), QString(tr("热键冲突")), QMessageBox::NoButton, QMessageBox::Close);
    }
}

void SettingsDialog::reply_advLeftButtonGroup_idToggled(int id, bool checked)
{
    if (checked) {
        switch (id) {
        case 0:
            ui->lineEdit_source_path_left->setText("");

            ui->lineEdit_width_left->setText(QString::number(0));
            ui->lineEdit_height_left->setText(QString::number(0));
            break;
        }
    }
}

void SettingsDialog::on_pushButton_source_select_left_clicked()
{
    int checkID = advTypeLeftButtonGroup->checkedId();
    switch (checkID) {
    case 0:
        ui->lineEdit_source_path_left->setText("");

        ui->lineEdit_width_left->setText(QString::number(0));
        ui->lineEdit_height_left->setText(QString::number(0));
        break;
    case 1: {
        //QString imgDir = QFileDialog::getExistingDirectory(this, QString(tr("选择图像文件夹")), GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT);
        //ui->lineEdit_source_path_left->setText(imgDir);
        //
        QString imgName = QFileDialog::getOpenFileName(this, QString(tr("选择图像文件")), GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT, "image(*.jpg *.png)");
        ui->lineEdit_source_path_left->setText(imgName);
    } break;
    case 2: {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("打开视频文件"),
            GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT,
            "video(*.mp4 *.flv *.f4v *.webm *.m4v *.mov *.3gp *.3g2 *.rm *.rmvb *.wmv *.avi *.asf *.mpg *.mpeg *.mpe *.ts *.div *.dv *.divx *.vob *.mkv)");
        ui->lineEdit_source_path_left->setText(fileName);
    } break;
    }
}

void SettingsDialog::reply_advRightButtonGroup_idToggled(int id, bool checked)
{
    if (checked) {
        switch (id) {
        case 0:
            ui->lineEdit_source_path_right->setText("");

            ui->lineEdit_width_right->setText(QString::number(0));
            ui->lineEdit_height_right->setText(QString::number(0));
            break;
        }
    }
}

void SettingsDialog::on_pushButton_source_select_right_clicked()
{
    int checkID = advTypeRightButtonGroup->checkedId();

    switch (checkID) {
    case 0:
        ui->lineEdit_source_path_right->setText("");

        ui->lineEdit_width_right->setText(QString::number(0));
        ui->lineEdit_height_right->setText(QString::number(0));
        break;
    case 1: {
        //QString imgDir = QFileDialog::getExistingDirectory(this, QString(tr("选择图像文件夹")), GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT);
        //ui->lineEdit_source_path_right->setText(imgDir);
        //
        QString imgName = QFileDialog::getOpenFileName(this, QString(tr("选择图像文件")), GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT, "image(*.jpg *.png)");
        ui->lineEdit_source_path_right->setText(imgName);
    } break;
    case 2: {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("打开视频文件"),
            GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT,
            "video(*.mp4 *.flv *.f4v *.webm *.m4v *.mov *.3gp *.3g2 *.rm *.rmvb *.wmv *.avi *.asf *.mpg *.mpeg *.mpe *.ts *.div *.dv *.divx *.vob *.mkv)");
        ui->lineEdit_source_path_right->setText(fileName);
    } break;
    }
}

void SettingsDialog::on_pushButton_dvertisement_confirm_clicked()
{
    int checkID_left = advTypeLeftButtonGroup->checkedId();
    GlobalDef::getInstance()->ADV_TYPE_LEFT = AdvType(checkID_left);
    GlobalDef::getInstance()->ADV_SOURCE_PATH_LEFT = ui->lineEdit_source_path_left->text();
    GlobalDef::getInstance()->ADV_WIDTH_LEFT = ui->lineEdit_width_left->text().toInt();
    GlobalDef::getInstance()->ADV_HEIGHT_LEFT = ui->lineEdit_height_left->text().toInt();

    int checkID_right = advTypeRightButtonGroup->checkedId();
    GlobalDef::getInstance()->ADV_TYPE_RIGHT = AdvType(checkID_right);
    GlobalDef::getInstance()->ADV_SOURCE_PATH_RIGHT = ui->lineEdit_source_path_right->text();
    GlobalDef::getInstance()->ADV_WIDTH_RIGHT = ui->lineEdit_width_right->text().toInt();
    GlobalDef::getInstance()->ADV_HEIGHT_RIGHT = ui->lineEdit_height_right->text().toInt();

    GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH = ui->lineEdit_width_3d->text().toInt();
    GlobalDef::getInstance()->PLAY_3D_VIEW_HEIGHT = ui->lineEdit_height_3d->text().toInt();

    if (checkID_left == 0 && checkID_right == 0) {
        GlobalDef::getInstance()->PLAY_3D_VIEW_WIDTH = 2;
        GlobalDef::getInstance()->PLAY_3D_VIEW_HEIGHT = 2;
    }

    ApplicationSettings appSettings;
    appSettings.write_WindowSizeState();

    QMessageBox::information(this, QString(tr("信息")), QString(tr("广告设置已保存，重启软件后生效")), QMessageBox::NoButton, QMessageBox::Close);

    close();
}

void SettingsDialog::on_pushButton_dvertisement_cancel_clicked()
{
    close();
}