#pragma once

#include <QtWidgets/QWidget>
#include "ui_MainWidget.h"
#include <QMediaPlayer>
#include <QFile>
#include <QEvent>

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private slots:
    // 弹出选择文件窗口
    void onFileDialogBtnClick();
    // 转换
    void onTransferBtnClick();
    // 保存文件
    void onSaveBtnClick();
    // 播放暂停
    void onPlayBtnClick();
    // 停止
    void onStopBtnClick();

private:
    void openFile();

signals:
    // 处理进度信号
    void progress(int v);

private:
    Ui::MainWidgetClass ui;
    QMediaPlayer *m_player = nullptr;
    QFile *m_file = nullptr;
};
