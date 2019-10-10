#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QTimer>
#include <QTime>
#include <QResizeEvent>
#include <QDebug>
#include <QProcess>

#include "LPF_V4L2.h"
#include "majorimageprocessingthread.h"

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private slots:
    void ReceiveMajorImage(QImage image, int result);

private:
    Ui::Widget *ui;

    int err11, err19;

    MajorImageProcessingThread *imageprocessthread;

    void InitWidget();

};

#endif // WIDGET_H
