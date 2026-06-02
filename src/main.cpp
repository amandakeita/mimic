#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <fstream>
#include <string>

using namespace std;

int openCam(int fd){
    fd=open("/dev/video0",O_RDWR);
    if(fd<0){
        cout<<" failed to open device ";
        return 1;
    }
    return fd;
}

int machineCheck(int fd){
    v4l2_capability capability;
    if(ioctl(fd,    VIDIOC_QUERYCAP, &capability)<0){
        cout<<" failed to understand device "<<VIDIOC_QUERYCAP;
        return 1;
    }
}

int formatSetting(int fd){
    v4l2_format imgFormat;
    imgFormat.fmt.pix.width = 1280;
    imgFormat.fmt.pix.height = 720;
    imgFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    imgFormat.fmt.pix.field = V4L2_FIELD_NONE;
    imgFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(ioctl(fd, VIDIOC_S_FMT, &imgFormat)<0){
        cout<<" unsuccessful format setting "<<VIDIOC_S_FMT;
        return 1;
    }
}


int main (){
    int fd;

    formatSetting(fd);
    machineCheck(fd);
    openCam(fd);

    return 0;
}