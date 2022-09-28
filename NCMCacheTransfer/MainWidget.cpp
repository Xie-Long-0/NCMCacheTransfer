#include "MainWidget.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>
#include <QAudioOutput>
#include <QMediaMetaData>
#include <QDropEvent>
#include <QMimeDatabase>
#include <QMimeData>

MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    setAcceptDrops(true);

    connect(ui.fileDialogBtn, &QPushButton::clicked, this, &MainWidget::onFileDialogBtnClick);
    connect(ui.transferBtn, &QPushButton::clicked, this, &MainWidget::onTransferBtnClick);
    connect(ui.saveBtn, &QPushButton::clicked, this, &MainWidget::onSaveBtnClick);
    connect(ui.playBtn, &QPushButton::clicked, this, &MainWidget::onPlayBtnClick);
    connect(ui.stopBtn, &QPushButton::clicked, this, &MainWidget::onStopBtnClick);

    ui.transferBtn->setEnabled(false);
    ui.saveBtn->setEnabled(false);
    ui.playBtn->setEnabled(false);
    ui.stopBtn->setEnabled(false);

    // 处理进度条
    connect(this, &MainWidget::progress, ui.progressBar, &QProgressBar::setValue);

    ui.outPathLineEdit->setText(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(new QAudioOutput(this));
    m_file = new QFile(this);

    // 播放进度变化
    connect(m_player, &QMediaPlayer::positionChanged, this, [this](int v) {
        v /= 1000;
        ui.playSlider->setValue(v);
        int min = v / 60;
        int sec = v % 60;
        ui.playedTimeLabel->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
        });
    // 播放总时长改变
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](int v) {
        v /= 1000;
        ui.playSlider->setMaximum(v);
        int min = v / 60;
        int sec = v % 60;
        ui.totalTimeLabel->setText(QString("%1:%2").arg(min).arg(sec, 2, 10, QChar('0')));
        });
    // 媒体信息改变
    connect(m_player, &QMediaPlayer::metaDataChanged, this, [this] {
        auto meta = m_player->metaData();
        ui.titleLine->setText(meta.stringValue(QMediaMetaData::Title));
        ui.artistLine->setText(meta.stringValue(QMediaMetaData::AlbumArtist));
        ui.albumLine->setText(meta.stringValue(QMediaMetaData::AlbumTitle));
        });
}

void MainWidget::dragEnterEvent(QDragEnterEvent *e)
{
    // 判断拖拽文件
    auto mime = e->mimeData();
    if (ui.fileLineEdit->geometry().contains(e->pos()) && mime->hasUrls())
    {
        auto fn = mime->urls()[0].fileName();
        if (fn.last(3) == ".uc" || fn.last(4) == ".uc!")
            e->acceptProposedAction();
    }
}

void MainWidget::dropEvent(QDropEvent * e)
{
    ui.fileLineEdit->setText(e->mimeData()->urls()[0].toLocalFile());
    e->acceptProposedAction();
    openFile();
}

void MainWidget::onFileDialogBtnClick()
{
    QString file_name = QFileDialog::getOpenFileName(this, tr("选择缓存文件"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation), "MCN Cache Files (*.uc *.uc!)");

    if (file_name.isEmpty())
        return;

    ui.fileLineEdit->setText(file_name);
    openFile();
}

void MainWidget::onTransferBtnClick()
{
    // 读取输入文件
    auto file_in = new QFile(ui.fileLineEdit->text(), this);
    if (!file_in->open(QIODevice::ReadOnly))
    {
        file_in->deleteLater();
        QMessageBox::critical(this, tr("错误"), tr("文件打开失败！"));
        return;
    }

    // 创建临时文件
    m_file->setFileName(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/tmp.music");
    if (!m_file->open(QIODevice::WriteOnly))
    {
        file_in->deleteLater();
        QMessageBox::critical(this, tr("错误"), tr("临时文件创建失败！"));
        return;
    }

    // 线程执行函数：对文件数据进行处理
    auto func = [this, file_in] {
        constexpr int LEN = 4096;  // 4KiB
        char buf[LEN] = {};
        qint64 n = 0;
        int len = 0;
        qint64 size = file_in->size();

        while (!file_in->atEnd())
        {
            len = file_in->read(buf, LEN);
            // 每个字节与0xa3异或
            for (int i = 0; i < len; ++i)
                buf[i] ^= 0xa3;

            m_file->write(buf, len);
            n += len;
            emit progress(100 * n / size);
        }
    };

    auto thread = QThread::create(func);

    connect(thread, &QThread::finished, this, [=] {
        QMessageBox::information(this, tr("提示"), tr("文件转换完成！"));

        m_player->setSource(m_file->fileName());
        if (m_player->mediaStatus() == QMediaPlayer::InvalidMedia)
        {
            QMessageBox::warning(this, tr("警告"), tr("无法识别的媒体文件。"));
        }
        ui.saveBtn->setEnabled(true);
        ui.playBtn->setEnabled(true);
        ui.transferBtn->setEnabled(false);

        file_in->deleteLater();
        thread->quit();
        thread->deleteLater();
        });

    thread->start();
}

void MainWidget::onSaveBtnClick()
{
    QDir out_dir = QDir::current();
    if (!out_dir.mkpath(ui.outPathLineEdit->text()))
    {
        QMessageBox::critical(this, tr("错误"), tr("无法创建输出文件路径！"));
        return;
    }

    // 获取文件实际类型
    auto mime = QMimeDatabase().mimeTypeForFile(m_file->fileName());

    QString file_name;
    // 以歌曲+歌手命名保存文件
    if (ui.renameCheckBox->isChecked())
    {
        if (!ui.artistLine->text().isEmpty())
            file_name = ui.artistLine->text();

        if (!ui.titleLine->text().isEmpty())
        {
            if (file_name.isEmpty())
                file_name = ui.titleLine->text();
            else
                file_name += " - " + ui.titleLine->text();
        }

        if (!file_name.isEmpty())
            file_name += "." + mime.preferredSuffix();
    }

    if (file_name.isEmpty())
        file_name = ui.fileNameLine->text().split('.')[0] + "." + mime.preferredSuffix();

    auto file_path = ui.outPathLineEdit->text() + '/' + file_name;

    if (QFile::exists(file_path))
    {
        auto ret = QMessageBox::warning(this, tr("警告"), tr("文件\"%1\"已存在，是否替换？").arg(file_path),
            QMessageBox::Yes, QMessageBox::No);
        if (ret == QMessageBox::Yes)
        {
            QFile::remove(file_path);
        }
        else
        {
            return;
        }
    }

    if (m_file->copy(file_path))
    {
        QMessageBox::information(this, tr("提示"), tr("文件保存成功：%1").arg(file_path));
        ui.fileNameLine->setText(file_name);
    }
    else
    {
        QMessageBox::warning(this, tr("警告"), tr("文件保存失败：%1").arg(file_path));
    }
}

void MainWidget::onPlayBtnClick()
{
    if (m_player->mediaStatus() == QMediaPlayer::InvalidMedia)
    {
        QMessageBox::warning(this, tr("错误"), tr("无法播放该文件"));
        return;
    }

    if (m_player->playbackState() != QMediaPlayer::PlayingState)
    {
        m_player->play();
        ui.playBtn->setText(tr("暂停"));
        ui.stopBtn->setEnabled(true);
    }
    else
    {
        m_player->pause();
        ui.playBtn->setText(tr("播放"));
        ui.stopBtn->setEnabled(true);
    }
}

void MainWidget::onStopBtnClick()
{
    m_player->stop();
    ui.playSlider->setValue(0);
    ui.playBtn->setText(tr("播放"));
    ui.stopBtn->setEnabled(false);
}

void MainWidget::openFile()
{
    onStopBtnClick();
    m_player->setSource(QUrl());
    ui.titleLine->setText("");
    ui.artistLine->setText("");
    ui.albumLine->setText("");
    m_file->close();

    QFileInfo file_info(ui.fileLineEdit->text());
    ui.fileNameLine->setText(file_info.fileName());

    double size = file_info.size();
    QString str_size;
    if (size <= 1024 * 1024) // KiB
    {
        str_size = QString("%1 KiB").arg(size / 1024, 0, 'f', 3);
    }
    else // MiB
    {
        str_size = QString("%1 MiB").arg(size / 1024 / 1024, 0, 'f', 3);
    }
    str_size.append(QString(" (%1 Bytes)").arg(file_info.size()));

    ui.fileSizeLine->setText(str_size);
    ui.transferBtn->setEnabled(true);
    ui.saveBtn->setEnabled(false);
    ui.playBtn->setEnabled(false);
    ui.progressBar->setValue(0);
}
