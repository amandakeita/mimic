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
        int bitsLeft;
        unsigned int bitCurrent;
        unsigned char* data;
        unsigned char* tracer;
        unsigned char counts [4][16];
        unsigned char symbols [4][162];
        unsigned int huffcodes[4][162];
        unsigned char hufflength[4][162];


    public:
        cameraHandling(){
            fd = -1;
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            bitsLeft =0;
            bitCurrent =0;
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

                data = (unsigned char*)buffer;
                for (int j =0; j<bufferInfo.bytesused-4;j++){
                    if(data[j]==0xFF && data[j+1]== 0xDB){//multipliers - Quantization Tables - scaling factors to reverse the compression math
                        memcpy(capTable, &data[j+5], 64);
                    } else if (data[j]==0xFF && data[j+1]== 0xC0){ //dimensions - height and width - to know the size of the 2D pixel grid im about to create
                        picHeight = (data[j+5] * 256) + data[j+6];
                        picWidth = (data[j+7] * 256) + data[j+8];





                    } else if (data[j]==0xFF && data[j+1]== 0xC4){                                      //huffman - dictionaries - to be able to read the "shorthand" bits in the image data
                        int length = (data[j+2])*256+data[j+3];
                        
                        int tableId = data[j+4];

                        int low = tableId&15;
                        int high = ( tableId>>4)&1;
                        // cout<<"low "<<low<<endl;
                        // cout<<"high "<<high<<endl;

                        int index = (high*2)+low;
                        memcpy(counts[index], &data[j+5], 16);

                        int sums = 0;
                        for(int i = 0; i<16;i++){
                            sums += counts[index][i];
                        }

                        // cout<<"first count from array counts "<<(int)counts[index][0]<<endl;
                        cout<<"sums "<<sums<<endl;

                        memcpy(symbols[index], &data[j+21], sums);

                        cout<<"last count from array symbols "<<(int)symbols[index][sums-1]<<endl;
                    

                    
                    } else if (data[j]==0xFF && data[j+1]== 0xDA){ //sos marker - start of Scan - tells the program exactly where the "Manual" ends and the "Image Data" begins
                        int sosLength = (data[j+2])*256+data[j+3];
                        int startPos = sosLength +j+2;
                        tracer = &data[startPos];
                        bitsLeft =0;
                        for(int test=0;test<8;test++){
                            readbit();
                        }
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

    int readbit(){
        int res=0;
        if(bitsLeft==0){
            bitCurrent = *tracer;
            bitsLeft=8;
            tracer++;
            if (bitCurrent==0xFF && *tracer== 0x00){
                tracer ++;
            } 
        } if (bitsLeft!=0) {
            int n = bitsLeft-1;
            res = (bitCurrent>>n)&1;
            bitsLeft -= 1;
        }
        // cout<<"result: "<<res<<endl;
        return res;
    }

    void huffmanTables(){

        for(int i=0;i<4;i++){
            int code = 0;
            int nextSymbol =0;
            for(int j=1;j<17;j++){
                if(counts[i][j]>0){
                    for(int n=0;n<counts[i][j];n++){
                        huffcodes[i][nextSymbol] = code;
                        hufflength[i][nextSymbol] = j;
                        code++;
                        cout << "Length: " << j << " | Code (decimal): " << code << " | Symbol: " << (int)symbols[i][nextSymbol] << endl;
                        nextSymbol++;
                    }
                }
                code = code<<1;
            }
        }
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
    cam.huffmanTables();
    cam.readbit();
    cam.endStream();

    return 0;
}

// 0x FFD8	SOI	Start of Image	
// 0x FFD9	EOI	End of Image	
// 0x FFDA	SOS	Start of Scan	
// 0x FFDB	DQT	Define Quantization Table
// 0x FFC4  Hufffman table