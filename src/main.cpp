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
        perror("failed to understand device, VIDIOC_QUERYCAP");
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
        perror("unsuccessful format setting, VIDIOC_S_FMT");
        return 1;
    }
}


int bufferPrep(int fd){
    v4l2_buffer queryBuffer = {0};
    queryBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    queryBuffer.memory = V4L2_MEMORY_MMAP;
    queryBuffer.index = 0;
    if(ioctl(fd, VIDIOC_QUERYBUF, &queryBuffer)<0){
        perror("buffer info wasnt returned");
        return 1;
    }

    char* buffer = (char*)mmap(NULL, queryBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, queryBuffer.m.offset);
    memset(buffer, 0, queryBuffer.length);
}

int bufferFrame(int fd){
    v4l2_buffer bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferInfo.memory = V4L2_MEMORY_MMAP;
    bufferInfo.index = 0;

    int type = bufferInfo.type;
    if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
        perror("Could not start streaming, VIDIOC_STREAMON");
        return 1;
    }
}

int captureLoop(int fd, int *bufferInfo){
    if(ioctl(fd, VIDIOC_QBUF, &bufferInfo)<0){
        perror("failed queuing buffer, VIDIOC_QBUF");
        return 1;
    }
    if(ioctl(fd, VIDIOC_DQBUF, &bufferInfo)<0){
        perror("failed dequeuing buffer, VIDIOC_DQBUF");
        return 1;
    }

    //  !
    ofstream outFile;
    outFile.open("webcam_output.jpeg", ios::binary | ios::app);

    int bufPos = 0;
    int ofMyBlocksize = 0;
}

int main (){
    int fd;

    bufferFrame(fd);
    bufferPrep(fd);
    formatSetting(fd);
    machineCheck(fd);
    openCam(fd);

    return 0;
}