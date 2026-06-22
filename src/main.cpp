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
#include <cmath>
#include <vector>
#define _USE_MATH_DEFINES

using namespace std;

class cameraHandling{

    private:
        int fd;
        void* buffer;
        struct v4l2_buffer bufferInfo;
        int type;
        unsigned char capTable[2][64];
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
        unsigned char huffSizes[4];
        const int mape [64] = {0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
                              12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
                              35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
                              58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
        int blockCoef [64];
        int precDC[3 ];
        double pixelRes[64];
        bool running = true;
        vector<unsigned char> frameBuffer;
        int y_h;
        int y_v; 
        int reIn = 0;

    public:
        cameraHandling(){
            fd = -1;
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            bitsLeft =0;
            bitCurrent =0;
            frameBuffer.resize(921600);
            y_h =1;
            y_v =1;
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

    int warmUp(){
        for (int i=0;i<10;i++){
            if(ioctl(fd, VIDIOC_DQBUF, &bufferInfo)<0){
                perror("failed dequeuing buffer, VIDIOC_DQBUF");
                return 1;
            }

            if(ioctl(fd, VIDIOC_QBUF, &bufferInfo)<0){
                perror("failed queuing buffer, VIDIOC_QBUF");
                return 1;
            }
        }
        return 0;
    }

    int captureLoop(){
        // ofstream outFile;
        // outFile.open("webcam_output.jpeg", ios::binary | ios::trunc);

        if(ioctl(fd, VIDIOC_DQBUF, &bufferInfo)<0){
                perror("failed dequeuing buffer, VIDIOC_DQBUF");
                return 1;
        }
        int bufPos = 0;
        int bufferLeft = bufferInfo.bytesused;

        // outFile.write(static_cast<char*>(buffer)+bufPos, bufferInfo.bytesused);

        data = (unsigned char*)buffer;
        for (int j =0; j<bufferInfo.bytesused-4;j++){
            if(data[j]==0xFF && data[j+1]== 0xDB){//multipliers - Quantization Tables - scaling factors to reverse the compression math
                int length = (data[j+2])*256+data[j+3];
                int bypro= 0;
                int id;
                while(bypro<length-2){
                    id = data[j+4+bypro];
                    for(int k =0;k<64;k++){
                        capTable[id][mape[k]] = data[j + 4 + bypro + 1 + k];
                    }
                    bypro +=65;
                }
                
            } else if (data[j]==0xFF && data[j+1]== 0xC0){ //dimensions - height and width - to know the size of the 2D pixel grid im about to create
                picHeight = (data[j+5] * 256) + data[j+6];
                picWidth = (data[j+7] * 256) + data[j+8];
                int datafi = data[j+11];
                unsigned char h = datafi>>4;
                unsigned char l = datafi&15;
                y_h=h;
                y_v=l;


            } else if (data[j]==0xFF && data[j+1]== 0xC4){ //huffman - dictionaries - to be able to read the "shorthand" bits in the image data
                int length = (data[j+2])*256+data[j+3];
                

                int bypro= 0;
                while(bypro<length-2){
                    int tableId = data[j+4+bypro];
                    int low = tableId&15;
                    int high = ( tableId>>4)&1;
                    int* p;
                    int index = (high*2)+low;

                    memcpy(counts[index], &data[j + 4 + bypro + 1], 16);
                    int sums = 0;
                    for(int i = 0; i<16;i++){
                        sums += counts[index][i];
                    }

                    huffSizes[index] = sums;

                    // cout<<"first count from array counts "<<(int)counts[index][0]<<endl;
                    // cout<<"sums "<<sums<<endl;

                    memcpy(symbols[index], &data[j + 4 + bypro + 1 + 16], sums);
                    bypro += 1 + 16 + sums;
                }
                



                // cout<<"last count from array symbols "<<(int)symbols[index][sums-1]<<endl;
            

            
            } else if (data[j]==0xFF && data[j+1]== 0xDA){ //sos marker - start of Scan - tells the program exactly where the "Manual" ends and the "Image Data" begins
                int sosLength = (data[j+2])*256+data[j+3];
                int startPos = sosLength +j+2;
                tracer = &data[startPos];
                bitsLeft =0;

                precDC[0] =0;
                precDC[1] =0;
                precDC[2] =0;
                bitsLeft = 0;
                bitCurrent = 0;
                resetter();
                huffmanTables();
                masterDecoder();
                break;
                // cout << "Starting decoding at byte: " << startPos << endl;
                // cout << "Luma DC Table Size: " << (int)huffSizes[0] << endl;
            } 

            else if (data[j]==0xFF && data[j+1]== 0xDD){
                reIn = (data[j+4]*256) + data[j+5];
            }

        }
        if(ioctl(fd, VIDIOC_QBUF, &bufferInfo)<0){
            perror("failed queuing buffer, VIDIOC_QBUF");
            return 1;
        }
// over all aims to map the memory and give it overseeable structure
                // outFile.close();

        return 0;
    }

    int readbit(){
        if(!running) return 0;
        int res=0;
        if(bitsLeft==0){
            while(true){
                bitCurrent = *tracer;

                static bool firstByte = true;
                if(firstByte){
                    firstByte = false;
                }

                tracer++;
                if(bitCurrent != 0xFF){
                    break;
                } else {
                    if(*tracer==0x00){
                        tracer++;
                        break;
                    } if(*tracer>=0xD0 && *tracer<=0xD7){
                        tracer ++;
                        precDC[0] = 0; 
                        precDC[1] = 0; 
                        precDC[2] = 0;
                        continue;
                    }if(*tracer==0xD9){
                        running = false;
                        return 0;
                    }
                }
            }
            if (running) bitsLeft =8;
            if(!running){
                return 0;
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
            for(int j=0;j<16;j++){
                if(counts[i][j]>0){
                    for(int n=0;n<counts[i][j];n++){
                        huffcodes[i][nextSymbol] = code;
                        hufflength[i][nextSymbol] = j+1;
                        // cout << "Length: " << j << " | Code (decimal): " << code << " | Symbol: " << (int)symbols[i][nextSymbol] << endl;
                        code++;
                        nextSymbol++;
                    }
                }
                code = code<<1;
            }
        }
    }

    int patternMatcher(int index){
        int curCode =0;
        int bitLength =0;
        int category =0;

        for (int i = 0;i<16;i++){
            int borrowed = readbit();
            curCode = (curCode<<1)+borrowed;
            bitLength++;

                for(int j =0;j<huffSizes[index];j++){
                    if (curCode == huffcodes[index][j] && bitLength == hufflength[index][j]){
                        // cout<<"match: "<<(int)symbols[index][j];
                        category = (int)symbols[index][j];
                        if(!running){
                            return 0;
                        }

                      
                        return symbols[index][j];


                    }
                }
            
        }
        running = false;
        cout << "no match found, table index = " << index << endl;
        for(int j =0;j<huffSizes[index];j++){
            cout<<"culprits; "<<(int)symbols[index][j]<<endl;
        }
        return 0;
    }

    int valueDecoder(int category){
        if(category ==0) return 0;
        int builder =0;
        int number =0;
        for(int i =0; i<category;i++){
            builder = readbit();
            number = (number<<1)+builder;
        }

        int firstBit = number>>(category-1);
        if(firstBit==1){
            return number;
        } else if(firstBit==0){
            number = number -((1<<category)-1);
            return number;
        }
    }
    
    int decoder(int dcTable, int acTable, int compID){
        memset(blockCoef, 0, sizeof(blockCoef));

        static int blockCount = 0;
        blockCount++;

        int res = patternMatcher(dcTable);
        int res2 = valueDecoder(res);
        unsigned char ore=0;
        unsigned char high =0;
        unsigned char low =0;
        int res3 = 0;
        int qIndex;


        precDC[compID] += res2;
        blockCoef[0] = precDC[compID];

        if(compID ==0) qIndex =0;
        else qIndex =1;

        for(int i=1; i<64;i++){
            ore = patternMatcher(acTable);
            if(ore ==0){
                break;
            } else if(ore==240){
                i += 15;
            } else{
                high = ore>>4;
                low = ore&15;
                i += high;
                if(i>63) break;
                res3 = valueDecoder(low);
                blockCoef[mape[i]]=res3;
            }
        }

        double spatialGrid[64];
        for(int j = 0;j<64;j++){
            spatialGrid[j] = blockCoef[j]*capTable[qIndex][j];
        }

        double cu=0.0;
        double cv=0.0;
        double cosValn =0, cosValk =0;
        for(int n=0;n<8;n++){
            for(int k=0;k<8;k++){
                    double sum =0.0;
                    for(int u=0;u<8;u++){
                        if(u==0) cu=1/sqrt(2);
                        if(u>0) cu=1.0;
                        cosValn = cos( ( (2 * n + 1) * u * M_PI ) / 16.0 );
                        for(int v=0;v<8;v++){
                            if(v==0) cv=1/sqrt(2);
                            if(v>0) cv=1.0;
                            cosValk = cos( ( (2 * k + 1) * v * M_PI ) / 16.0 );
                            sum += cu*cv*cosValk*cosValn*spatialGrid[u * 8 + v];
                    }
                }
                double temp = (sum/4.0)+128.5;

                if(temp>255.00) temp = 255;
                if(temp<0.0) temp = 0;
                pixelRes[n * 8 + k] = (unsigned char)temp;

            }

        }

        for(int r = 0;r<8;r++){
            for(int c=0;c<8;c++){
                // cout<<(int)pixelRes[r * 8 + c]<<endl;
            }
        }

        return 0;
    }

    int masterDecoder(){
        precDC[0] = 0; 
        precDC[1] = 0; 
        precDC[2] = 0;
        int mcuW = y_h*8;
        int mcuH = y_v*8;
        int counter =0;
        int globalX=0, globalY =0, finalLoc =0;
        for(int i = 0;i<picHeight/mcuH;i++){
            for(int j = 0;j<picWidth/mcuW;j++){
                for(int ij=0;ij<y_v;ij++){ 
                for(int ii=0;ii<y_h;ii++){ 
                    decoder(0,2,0); 
                    if(!running){
                        break;
                        return 0;
                    } 
                    for(int u=0;u<8;u++){
                        for(int v = 0;v<8;v++){
                                    int gx = (j * mcuW) + (ii*8) +v;
                                    int gy = (i * mcuH) + (ij*8) + u;
                                    int finalIndex = (gy * picWidth) + gx;
                                    frameBuffer[finalIndex] = pixelRes[u * 8 + v];
                                }
                            }
                        }
                    }
                decoder(1,3,1);
                decoder(1,3,2);
                if(!running){
                    break;
                    return 0;
                }
                counter++;
                if(reIn > 0 && counter % reIn == 0){
                    bitsLeft = 0;
                }  
            }
        }
        cout << "Frame Decoded Successfully at index: " << (void*)tracer << endl;
        // ofstream testFile("out.pgm", ios::binary);
        // testFile<<"P5\n" << picWidth << " " << picHeight << "\n255\n";
        // testFile.write((char*)frameBuffer.data(), frameBuffer.size());
        // testFile.close();
        return 0;
    }

    int resetter(){
        bitsLeft =0;
        bitCurrent =0;
        precDC[0] =0;
        precDC[1] =0;
        precDC[2] =0;
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

    bool running = true;

    cameraHandling cam;

    cam.openCam();
    cam.machineCheck();
    cam.formatSetting();
    cam.bufferPrep();
    cam.bufferFrame();
    cam.warmUp();
    while(running){
        cam.captureLoop();
    }
    cam.endStream();


    return 0;
}

// 0x FFD8	SOI	Start of Image	
// 0x FFD9	EOI	End of Image	
// 0x FFDA	SOS	Start of Scan	
// 0x FFDB	DQT	Define Quantization Table
// 0x FFC4  Hufffman table