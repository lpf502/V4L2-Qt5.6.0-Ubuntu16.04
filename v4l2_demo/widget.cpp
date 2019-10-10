#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);

    InitWidget();
}

Widget::~Widget()
{
    delete ui;
}

void Widget::InitWidget()
{
    err11 = err19 = 0;

    imageprocessthread = new MajorImageProcessingThread;

    ui->frame1->setLayout(ui->gridLayout2);

    setWindowTitle("V4L2_Demo Test...");
    setFixedSize(690, 390);
    setLayout(ui->gridLayout1);

    int camcount = LPF_GetDeviceCount();
    if(camcount > 0)
    {
        //获取所有设备
        for(int i = 0; i < camcount; i++)
            ui->camComboBox->addItem(QString::fromLatin1(LPF_GetCameraName(i)));

        //启动默认视频
        LPF_StartRun(0);
        imageprocessthread->init(ui->camComboBox->currentIndex());
        imageprocessthread->start();

        QString curTime = QTime::currentTime().toString("hh:mm:ss");
        QString msg = curTime + QString(":") + QString("打开设备->") + ui->camComboBox->currentText() + QString("\n");
        ui->showMSG->insertPlainText(msg);

        //获取所有分辨率
        ui->resComboBox->clear();
        int rescount = LPF_GetResolutinCount();
        int curW = LPF_GetCurResWidth();
        int curH = LPF_GetCurResHeight();
        for(int i = 0; i < rescount; i++)
        {
            QString rW = QString::number(LPF_GetResolutionWidth(i));
            QString rH = QString::number(LPF_GetResolutionHeight(i));
            QString res = rW + "X" + rH;
            ui->resComboBox->addItem(res);
            bool ok;
            if(curW == rW.toInt(&ok, 10) && curH == rH.toInt(&ok, 10))
                ui->resComboBox->setCurrentIndex(i);
        }

        //获取视频格式
        ui->formatComboBox->clear();
        int i = 0;
        QString format;
        while(true)
        {
            format = QString::fromLatin1(LPF_GetDevFmtDesc(i++));
            if(errno == EINVAL)
                break;
            ui->formatComboBox->addItem(format);
        }

        ui->camComboBox->setToolTip(ui->camComboBox->currentText());
        ui->resComboBox->setToolTip(ui->resComboBox->currentText());
        ui->formatComboBox->setToolTip(ui->formatComboBox->currentText());
    }
    else
    {
        ui->mainlabel->clear();
        ui->mainlabel->setText("无设备被检测到！");
    }

    connect(imageprocessthread, SIGNAL(SendMajorImageProcessing(QImage, int)),
            this, SLOT(ReceiveMajorImage(QImage, int)));
}

void Widget::ReceiveMajorImage(QImage image, int result)
{
    //超时后关闭视频
    //超时代表着VIDIOC_DQBUF会阻塞，直接关闭视频即可
    if(result == -1)
    {
        imageprocessthread->stop();
        imageprocessthread->wait();
        LPF_StopRun();

        ui->mainlabel->clear();
        ui->mainlabel->setText("获取设备图像超时！");
    }

    if(!image.isNull())
    {
        ui->mainlabel->clear();
        switch(result)
        {
        case 0:     //Success
            err11 = err19 = 0;
            if(image.isNull())
                ui->mainlabel->setText("画面丢失！");
            else
                ui->mainlabel->setPixmap(QPixmap::fromImage(image.scaled(ui->mainlabel->size())));

            break;
        case 11:    //Resource temporarily unavailable
            err11++;
            if(err11 == 10)
            {
                ui->mainlabel->clear();
                ui->mainlabel->setText("设备已打开，但获取视频失败！\n请尝试切换USB端口后断开重试！");
            }
            break;
        case 19:    //No such device
            err19++;
            if(err19 == 10)
            {
                ui->mainlabel->clear();
                ui->mainlabel->setText("设备丢失！");
            }
            break;
        }
    }
}


