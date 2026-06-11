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

class cameraHandling{

    private:
        int fd;
        void* buffer;
        struct v4l2_buffer bufferInfo;
        int type;

    public:
        cameraHandling(){

            fd = -1;
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        }


    int openCam(){
        fd=open("/dev/video0",O_RDWR);
        if(fd<0){
            cout<<" failed to open device ";
            return 1;
        } 
        return 0;
    }

    int machineCheck(){
        v4l2_capability capability;
        if(ioctl(fd, VIDIOC_QUERYCAP, &capability)<0){
            perror("failed to understand device, VIDIOC_QUERYCAP");
            return 1;
        }
        return 0;
    }

    int formatSetting(){
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
        return 0;
    }


    int bufferPrep(){

        struct v4l2_requestbuffers bufReq = {0};
        bufReq.count =1;
        bufReq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufReq.memory = V4L2_MEMORY_MMAP;

        if(ioctl(fd, VIDIOC_REQBUFS, &bufReq)<0){
            perror("REQBUFS info wasnt returned");
            return 1;
        }

        v4l2_buffer queryBuffer = {0};
        queryBuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        queryBuffer.memory = V4L2_MEMORY_MMAP;
        queryBuffer.index = 0;
        if(ioctl(fd, VIDIOC_QUERYBUF, &queryBuffer)<0){
            perror("QUERYBUF info wasnt returned");
            return 1;
        }

        buffer = mmap(NULL, queryBuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, queryBuffer.m.offset);
        memset(buffer, 0, queryBuffer.length);
        
        return 0;
    }

    int bufferFrame(){
        memset(&bufferInfo, 0, sizeof(bufferInfo));
        bufferInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        bufferInfo.memory = V4L2_MEMORY_MMAP;
        bufferInfo.index = 0;

        if(ioctl(fd, VIDIOC_QBUF, &bufferInfo)<0){
            perror("failed queuing buffer, VIDIOC_QBUF");
            return 1;
        }

        if(ioctl(fd, VIDIOC_STREAMON, &type) < 0){
            perror("Could not start streaming, VIDIOC_STREAMON");
            return 1;
        }
        return 0;
    }

    int captureLoop(){

        for (int i=0;i<10;i++){
            if(ioctl(fd, VIDIOC_DQBUF, &bufferInfo)<0){
            perror("failed dequeuing buffer, VIDIOC_DQBUF");
            return 1;
            }
            if (i==9){
                ofstream outFile;
                outFile.open("webcam_output.jpeg", ios::binary | ios::trunc);

                int bufPos = 0;
                int bufferLeft = bufferInfo.bytesused;

                outFile.write(static_cast<char*>(buffer)+bufPos, bufferInfo.bytesused);

                outFile.close();
            }
            if(ioctl(fd, VIDIOC_QBUF, &bufferInfo)<0){
            perror("failed queuing buffer, VIDIOC_QBUF");
            return 1;
            }
        }

        return 0;
    }

    int endStream(){
        if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
            perror("Could not end streaming, VIDIOC_STREAMOFF");
            return 1;
        }
        close(fd);
        return 0;
    }
};

int main (){

    cameraHandling cam;

    cam.openCam();
    cam.machineCheck();
    cam.formatSetting();
    cam.bufferPrep();
    cam.bufferFrame();
    cam.captureLoop();
    cam.endStream();

    return 0;
}