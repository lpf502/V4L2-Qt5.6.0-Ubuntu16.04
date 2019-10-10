#include "majorimageprocessingthread.h"

MajorImageProcessingThread::MajorImageProcessingThread()
{
    stopped = false;
    majorindex = -1;
}

void MajorImageProcessingThread::stop()
{
    stopped = true;
}

void MajorImageProcessingThread::init(int index)
{
    stopped = false;
    majorindex = index;
}

void MajorImageProcessingThread::run()
{
    if(majorindex != -1)
    {
        while(!stopped)
        {
            msleep(1000/30);

            QImage img;
            int ret = LPF_GetFrame();
            if(ret == 0)
            {
                int WV = LPF_GetCurResWidth();
                int HV = LPF_GetCurResHeight();
                img = QImage(rgb24, WV, HV, QImage::Format_RGB888);
            }

            emit SendMajorImageProcessing(img, ret);
        }
    }
}

