#include "LPF_V4L2.h"

#ifdef __cplusplus
extern "C" {
#endif

static struct buffer{
    void *start;
    unsigned int length;
}*buffers;
int buffers_length;

char runningDev[15] = "";
char devName[15] = "";
char camName[32] = "";
char devFmtDesc[4] = "";

int fd = -1;
int videoIsRun = -1;
int deviceIsOpen = -1;
unsigned char *rgb24 = NULL;
static int WIDTH, HEIGHT;

//V4l2相关结构体
static struct v4l2_capability cap;
static struct v4l2_fmtdesc fmtdesc;
static struct v4l2_frmsizeenum frmsizeenum;
static struct v4l2_format format;
static struct v4l2_queryctrl  queryctrl;
static struct v4l2_requestbuffers reqbuf;
static struct v4l2_buffer buffer;
static struct v4l2_streamparm streamparm;

//用户控制项ID
static __u32 brightness_id = -1;    //亮度
static __u32 contrast_id = -1;  //对比度
static __u32 saturation_id = -1;    //饱和度
static __u32 hue_id = -1;   //色调
static __u32 white_balance_temperature_auto_id = -1; //白平衡色温（自动）
static __u32 white_balance_temperature_id = -1; //白平衡色温
static __u32 gamma_id = -1; //伽马
static __u32 power_line_frequency_id = -1;  //电力线频率
static __u32 sharpness_id = -1; //锐度，清晰度
static __u32 backlight_compensation_id = -1;    //背光补偿
//扩展摄像头项ID
static __u32 exposure_auto_id = -1;  //自动曝光
static __u32 exposure_absolute_id = -1;

void StartVideoPrePare();
void StartVideoStream();
void EndVideoStream();
void EndVideoStreamClear();
int test_device_exist(char *devName);

static int convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned int pixel32 = 0;
    unsigned char *pixel = (unsigned char *)&pixel32;
    int r, g, b;
    r = y + (1.370705 * (v-128));
    g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
    b = y + (1.732446 * (u-128));
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;
    pixel[0] = r ;
    pixel[1] = g ;
    pixel[2] = b ;
    return pixel32;
}

static int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int in, out = 0;
    unsigned int pixel_16;
    unsigned char pixel_24[3];
    unsigned int pixel32;
    int y0, u, y1, v;

    for(in = 0; in < width * height * 2; in += 4)
    {
        pixel_16 =
                yuv[in + 3] << 24 |
                               yuv[in + 2] << 16 |
                                              yuv[in + 1] <<  8 |
                                                              yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;
        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;
        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }
    return 0;
}

void StartVideoPrePare()
{
    //申请帧缓存区
    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 4;

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL)
            printf ("Video capturing or mmap-streaming is not supported\n");
        else
            perror ("VIDIOC_REQBUFS");
        return;
    }

    //分配缓存区
    buffers = calloc (reqbuf.count, sizeof (*buffers));
    if(buffers == NULL)
        perror("buffers is NULL");
    else
        assert (buffers != NULL);

    //mmap内存映射
    int i;
    for (i = 0; i < (int)reqbuf.count; i++) {
        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)) {
            perror ("VIDIOC_QUERYBUF");
            return;
        }

        buffers[i].length = buffer.length;

        buffers[i].start = mmap (NULL, buffer.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 fd, buffer.m.offset);

        if (MAP_FAILED == buffers[i].start) {
            perror ("mmap");
            return;
        }
    }

    //将缓存帧放到队列中等待视频流到来
    unsigned int ii;
    for(ii = 0; ii < reqbuf.count; ii++){
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = ii;
        if (ioctl(fd,VIDIOC_QBUF,&buffer)==-1){
            perror("VIDIOC_QBUF failed");
        }
    }

    WIDTH = LPF_GetCurResWidth();
    HEIGHT = LPF_GetCurResHeight();
    rgb24 = (unsigned char*)malloc(WIDTH*HEIGHT*3*sizeof(char));
    assert(rgb24 != NULL);
}

void StartVideoStream()
{
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMON,&type) == -1) {
        perror("VIDIOC_STREAMON failed");
    }
}

void EndVideoStream()
{
    //关闭视频流
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd,VIDIOC_STREAMOFF,&type) == -1) {
        perror("VIDIOC_STREAMOFF failed");
    }
}

void EndVideoStreamClear()
{
    //手动释放分配的内存
    int i;
    for (i = 0; i < (int)reqbuf.count; i++)
        munmap (buffers[i].start, buffers[i].length);
    free(rgb24);
    rgb24 = NULL;
}

void LPF_GetDevControlAll()
{
    int i = 0;
    for(i = V4L2_CID_BASE; i <= V4L2_CID_LASTP1; i++)
    {
        queryctrl.id = i;
        if(0 == ioctl(fd, VIDIOC_QUERYCTRL, &queryctrl))
        {
            if(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

            if(queryctrl.id == V4L2_CID_BRIGHTNESS)
                brightness_id = i;
            if(queryctrl.id == V4L2_CID_CONTRAST)
                contrast_id = i;
            if(queryctrl.id == V4L2_CID_SATURATION)
                saturation_id = i;
            if(queryctrl.id == V4L2_CID_HUE)
                hue_id = i;
            if(queryctrl.id == V4L2_CID_AUTO_WHITE_BALANCE)
                white_balance_temperature_auto_id = i;
            if(queryctrl.id == V4L2_CID_WHITE_BALANCE_TEMPERATURE)
                white_balance_temperature_id = i;
            if(queryctrl.id == V4L2_CID_GAMMA)
                gamma_id = i;
            if(queryctrl.id == V4L2_CID_POWER_LINE_FREQUENCY)
                power_line_frequency_id = i;
            if(queryctrl.id == V4L2_CID_SHARPNESS)
                sharpness_id = i;
            if(queryctrl.id == V4L2_CID_BACKLIGHT_COMPENSATION)
                backlight_compensation_id = i;
        }
        else
        {
            if(errno == EINVAL)
                continue;
            perror("VIDIOC_QUERYCTRL");
            return;
        }
    }

    queryctrl.id = V4L2_CTRL_CLASS_CAMERA | V4L2_CTRL_FLAG_NEXT_CTRL;
    while (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
        if (V4L2_CTRL_ID2CLASS (queryctrl.id) != V4L2_CTRL_CLASS_CAMERA)
            break;

        if(queryctrl.id == V4L2_CID_EXPOSURE_AUTO)
            exposure_auto_id = queryctrl.id;
        if(queryctrl.id == V4L2_CID_EXPOSURE_ABSOLUTE)
            exposure_absolute_id = queryctrl.id;

        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }
}

int test_device_exist(char *devName)
{
    struct stat st;
    if (-1 == stat(devName, &st))
        return -1;

    if (!S_ISCHR (st.st_mode))
        return -1;

    return 0;
}
int LPF_GetDeviceCount()
{
    char devname[15] = "";
    int count = 0;
    int i;
    for(i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if(test_device_exist(devname) == 0)
            count++;

        memset(devname, 0, sizeof(devname));
    }

    return count;
}

//根据索引获取设备名称
char *LPF_GetDeviceName(int index)
{
    memset(devName, 0, sizeof(devName));

    int count = 0;
    char devname[15] = "";
    int i;
    for(i = 0; i < 100; i++)
    {
        sprintf(devname, "%s%d", "/dev/video", i);
        if(test_device_exist(devname) == 0)
        {
            if(count == index)
                break;
            count++;
        }
        else
            memset(devname, 0, sizeof(devname));
    }

    strcpy(devName, devname);

    return devName;
}

//根据索引获取摄像头名称
char *LPF_GetCameraName(int index)
{
    if(videoIsRun > 0)
        return "";

    memset(camName, 0, sizeof(camName));

    char devname[15] = "";
    strcpy(devname, LPF_GetDeviceName(index));

    int fd = open(devname, O_RDWR);
    if(ioctl(fd, VIDIOC_QUERYCAP, &cap) != -1)
    {
        strcpy(camName, (char *)cap.card);
    }
    close(fd);

    return camName;
}

//运行指定索引的视频
int LPF_StartRun(int index)
{
    if(videoIsRun > 0)
        return -1;

    char *devname = LPF_GetDeviceName(index);
    fd = open(devname, O_RDWR);
    if(fd == -1)
        return -1;

    deviceIsOpen = 1;

    LPF_GetDevControlAll();

    StartVideoPrePare();
    StartVideoStream();

    strcpy(runningDev, devname);
    videoIsRun = 1;

    return 0;
}

int LPF_GetFrame()
{
    if(videoIsRun > 0)
    {
        fd_set fds;
        struct timeval tv;
        int r;

        FD_ZERO (&fds);
        FD_SET (fd, &fds);

        /* Timeout. */
        tv.tv_sec = 7;
        tv.tv_usec = 0;

        r = select (fd + 1, &fds, NULL, NULL, &tv);

        if (0 == r)
            return -1;
        else if(-1 == r)
            return errno;

        memset(&buffer, 0, sizeof(buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd, VIDIOC_DQBUF, &buffer) == -1) {
            perror("GetFrame VIDIOC_DQBUF Failed");
            return errno;
        }
        else
        {
            convert_yuv_to_rgb_buffer((unsigned char*)buffers[buffer.index].start, rgb24, WIDTH, HEIGHT);

            if (ioctl(fd, VIDIOC_QBUF, &buffer) < 0) {
                perror("GetFrame VIDIOC_QBUF Failed");
                return errno;
            }

            return 0;
        }
    }

    return 0;
}

int LPF_StopRun()
{
    if(videoIsRun > 0)
    {
        EndVideoStream();
        EndVideoStreamClear();
    }

    memset(runningDev, 0, sizeof(runningDev));
    videoIsRun = -1;
    deviceIsOpen = -1;

    if(close(fd) != 0)
        return -1;

    return 0;
}

char *LPF_GetDevFmtDesc(int index)
{
    memset(devFmtDesc, 0, sizeof(devFmtDesc));

    fmtdesc.index=index;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) != -1)
    {
        char fmt[5] = "";
        sprintf(fmt, "%c%c%c%c",
                (__u8)(fmtdesc.pixelformat&0XFF),
                (__u8)((fmtdesc.pixelformat>>8)&0XFF),
                (__u8)((fmtdesc.pixelformat>>16)&0XFF),
                (__u8)((fmtdesc.pixelformat>>24)&0XFF));

        strncpy(devFmtDesc, fmt, 4);
    }

    return devFmtDesc;
}

//获取图像的格式属性相关
int LPF_GetDevFmtWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtWidth:");
        return -1;
    }
    return format.fmt.pix.width;
}
int LPF_GetDevFmtHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtHeight:");
        return -1;
    }
    return format.fmt.pix.height;
}
int LPF_GetDevFmtSize()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtSize:");
        return -1;
    }
    return format.fmt.pix.sizeimage;
}
int LPF_GetDevFmtBytesLine()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl (fd, VIDIOC_G_FMT, &format) == -1)
    {
        perror("GetDevFmtBytesLine:");
        return -1;
    }
    return format.fmt.pix.bytesperline;
}

//设备分辨率相关
int LPF_GetResolutinCount()
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;
    int i = 0;
    for(i = 0; ; i++)
    {
        frmsizeenum.index = i;
        if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) == -1)
            break;
    }
    return i;
}
int LPF_GetResolutionWidth(int index)
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;

    frmsizeenum.index = index;
    if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) != -1)
        return frmsizeenum.discrete.width;
    else
        return -1;
}
int LPF_GetResolutionHeight(int index)
{
    fmtdesc.index = 0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1)
        return -1;

    frmsizeenum.pixel_format = fmtdesc.pixelformat;

    frmsizeenum.index = index;
    if(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsizeenum) != -1)
        return frmsizeenum.discrete.height;
    else
        return -1;
}
int LPF_GetCurResWidth()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.width;
}
int LPF_GetCurResHeight()
{
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd, VIDIOC_G_FMT, &format) == -1)
        return -1;
    return format.fmt.pix.height;
}

#ifdef __cplusplus
}
#endif


