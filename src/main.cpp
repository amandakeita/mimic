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
#include <cstring> 

using namespace std;

class cameraHandling{

    private:
        int fd;
        void* buffer;
        struct v4l2_buffer bufferInfo;
        int type;
        unsigned char capTable[64];
        int picWidth=0;
        int picHeight=0;

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

                unsigned char* data = (unsigned char*)buffer;
                for (int j =0; j<bufferInfo.bytesused-4;j++){
                    if(data[j]==0xFF && data[j+1]== 0xDB){//multipliers - Quantization Tables - scaling factors to reverse the compression math
                        // cout<<hex<<"j position at: "<<j<<" for "<<(int)data[j]<<" ";
                        // cout<<hex<<"j+1 position: "<<(int)data[j+1]<<" ";
                        // cout<<dec<<"value of two bytes following: "<<(int)data[j+2]+(int)data[j+3]<<" ";
                        // cout<<dec<<"total  lenght: "<< (data[j+2])*256+data[j+3]<<" ";
                        // cout<<"luminor table id "<<(int)data[j+4];
                        memcpy(capTable, &data[j+5], 64);
                        // cout<<"cap table at 0 "<<(int)capTable[0]<<" ";
                        // cout<<"cap table at 63 "<<(int)capTable[63]<<" ";
                        // cout<<"data[j + 5 + 63]: "<<(int)data[j + 5 + 63]<<" ";
                    } else if (data[j]==0xFF && data[j+1]== 0xC0){ //dimensions - height and width - to know the size of the 2D pixel grid im about to create
                        picHeight = (data[j+5] * 256) + data[j+6];
                        picWidth = (data[j+7] * 256) + data[j+8];
                        // cout<<"picwidth"<<picWidth<<" ";
                        // cout<<"picheight"<<picHeight<<" ";
                    } else if (data[j]==0xFF && data[j+1]== 0xC4){ //huffman - dictionaries - to be able to read the "shorthand" bits in the image data
                        int length = (data[j+2])*256+data[j+3];
                        cout<<"length "<<length<<" ";
                    } else if (data[j]==0xFF && data[j+1]== 0xDA){ //sos marker - start of Scan - tells the program exactly where the "Manual" ends and the "Image Data" begins
                        int sosLength = (data[j+2])*256+data[j+3];
                        int startPos = sosLength +j+2;
                        cout<<"length: "<<sosLength<<" "<<" image position when starting: "<<startPos;
                    }
                }
// over all aims to map the memory and give it overseeable structure
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

// 0x FFD8	SOI	Start of Image	
// 0x FFD9	EOI	End of Image	
// 0x FFDA	SOS	Start of Scan	
// 0x FFDB	DQT	Define Quantization Table
// 0x FFC4  Hufffman table