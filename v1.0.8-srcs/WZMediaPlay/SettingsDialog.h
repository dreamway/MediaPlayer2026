#pragma once
#include <QButtonGroup>
#include <QDialog>

namespace Ui {
class SettingsDialog;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

signals:
    void updateLanguage();
    void hotKeyChanged();
    void subtitleSettingsChanged();

private:
    bool checkHotKey(QString item, QString editKey);

private slots:
    //    系统设置
    void on_comboBox_language_activated(int index);
    void on_pushButton_splash_logo_clicked();
    void on_pushButton_play_window_logo_clicked();
    void on_pushButton_menu_icon_logo_clicked();

    void on_pushButton_system_setting_confirm_clicked();
    void on_pushButton_system_setting_cancel_clicked();

    //    播放设置
    void reply_moveWindowSizeButtonGroup_idToggled(int id, bool checked);
    void on_pushButton_play_setting_confirm_clicked();
    void on_pushButton_play_setting_cancel_clicked();

    //    图片设置
    void on_pushButton_screenshot_select_path_clicked();
    void on_pushButton_img_setting_confirm_clicked();
    void on_pushButton_img_setting_cancel_clicked();

    //    字幕设置
    void on_checkBox_auto_load_same_name_stateChanged(int state);
    void on_pushButton_subtitle_select_clicked();
    void on_pushButton_subtitle_setting_confirm_clicked();
    void on_pushButton_subtitle_setting_cancel_clicked();

    //    声音设置
    void on_comboBox_voice_select_activated(int index);
    void on_pushButton_voice_setting_confirm_clicked();
    void on_pushButton_voice_setting_cancel_clicked();

    //    鼠标热键--文件
    void on_pushButton_hotkey_file_restore_default_clicked();
    void on_pushButton_hotkey_file_cancel_clicked();
    void on_pushButton_hotkey_file_confirm_clicked();

    void on_keySequenceEdit_hotkey_file_openfile_keySequenceChanged();
    void on_keySequenceEdit_hotkey_file_openfile_editingFinished();
    void on_keySequenceEdit_hotkey_file_closefile_editingFinished();
    void on_keySequenceEdit_hotkey_file_previous_editingFinished();
    void on_keySequenceEdit_hotkey_file_next_editingFinished();

    //    鼠标热键--播放
    void on_pushButton_hotkey_play_restore_default_clicked();
    void on_pushButton_hotkey_play_cancel_clicked();
    void on_pushButton_hotkey_play_confirm_clicked();

    void on_keySequenceEdit_hotkey_play_playpause_editingFinished();
    void on_keySequenceEdit_hotkey_play_2D3D_editingFinished();
    void on_keySequenceEdit_hotkey_play_LR_editingFinished();
    void on_keySequenceEdit_hotkey_play_RL_editingFinished();
    void on_keySequenceEdit_hotkey_play_UD_editingFinished();
    void on_keySequenceEdit_hotkey_play_vertical_editingFinished();
    void on_keySequenceEdit_hotkey_play_horizontal_editingFinished();
    void on_keySequenceEdit_hotkey_play_chess_editingFinished();
    void on_keySequenceEdit_hotkey_play_region_editingFinished();

    //    鼠标热键--图像
    void on_pushButton_hotkey_image_restore_default_clicked();
    void on_pushButton_hotkey_image_cancel_clicked();
    void on_pushButton_hotkey_image_confirm_clicked();

    //void on_keySequenceEdit_hotkey_image_fullscreen_shot_editingFinished();
    void on_keySequenceEdit_hotkey_image_screenshot_editingFinished();

    //    鼠标热键--声音
    void on_pushButton_hotkey_voice_restore_default_clicked();
    void on_pushButton_hotkey_voice_cancel_clicked();
    void on_pushButton_hotkey_voice_confirm_clicked();

    void on_keySequenceEdit_hotkey_voice_volup_editingFinished();
    void on_keySequenceEdit_hotkey_voice_volde_editingFinished();
    void on_keySequenceEdit_hotkey_voice_mute_editingFinished();

    //    鼠标热键--字幕
    void on_pushButton_hotkey_subtitle_restore_default_clicked();
    void on_pushButton_hotkey_subtitle_cancel_clicked();
    void on_pushButton_hotkey_subtitle_confirm_clicked();

    void on_keySequenceEdit_hotkey_subtitle_loadsubtitle_editingFinished();
    void on_keySequenceEdit_hotkey_subtitle_changesubtitle_editingFinished();

    //    鼠标热键--其它
    void on_pushButton_hotkey_others_restore_default_clicked();
    void on_pushButton_hotkey_others_cancel_clicked();
    void on_pushButton_hotkey_others_confirm_clicked();

    void on_keySequenceEdit_hotkey_others_playlist_editingFinished();
    void on_keySequenceEdit_hotkey_others_fullscreen_plus_editingFinished();
    void on_keySequenceEdit_hotkey_others_fullscreen_editingFinished();
    void on_keySequenceEdit_hotkey_others_increase_parallax_editingFinished();
    void on_keySequenceEdit_hotkey_others_decrease_parallax_editingFinished();
    void on_keySequenceEdit_hotkey_others_reset_parallax_editingFinished();

    //    广告设置
    void reply_advLeftButtonGroup_idToggled(int id, bool checked);
    void on_pushButton_source_select_left_clicked();
    void reply_advRightButtonGroup_idToggled(int id, bool checked);
    void on_pushButton_source_select_right_clicked();
    void on_pushButton_dvertisement_confirm_clicked();
    void on_pushButton_dvertisement_cancel_clicked();

private:
    Ui::SettingsDialog *ui;
    QButtonGroup *moveWindowSizeButtonGroup;
    //QButtonGroup* screenshotButtonGroup;
    QButtonGroup *advTypeLeftButtonGroup;
    QButtonGroup *advTypeRightButtonGroup;
    int MOVE_FIT_WINDOW_SATAE_TEMP;
};