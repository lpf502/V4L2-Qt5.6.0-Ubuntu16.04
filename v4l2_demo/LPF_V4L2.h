#ifndef  __MAJORV4L2_H_
#define __MAJORV4L2_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *rgb24;
extern int videoIsRun;

int LPF_GetDeviceCount();
char *LPF_GetDeviceName(int index);
char *LPF_GetCameraName(int index);

int LPF_StartRun(int index);
int LPF_GetFrame();
int LPF_StopRun();

char *LPF_GetDevFmtDesc(int index);

int LPF_GetDevFmtWidth();
int LPF_GetDevFmtHeight();
int LPF_GetDevFmtSize();
int LPF_GetDevFmtBytesLine();

int LPF_GetResolutinCount();
int LPF_GetResolutionWidth(int index);
int LPF_GetResolutionHeight(int index);
int LPF_GetCurResWidth();
int LPF_GetCurResHeight();

#ifdef __cplusplus
}
#endif
#endif

