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
#include <SDL2/SDL.h>
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
        bool running;
        vector<unsigned char> frameBuffer;
        int y_h;
        int y_v; 
        int reIn = 0;
        SDL_Window* window;
        SDL_Renderer* renderer;
        SDL_Surface* surface;
        SDL_Texture* texture;
        SDL_Texture* happy;
        bool done = false;
        double cosTable[8][8];
        int right =840;
        int left =440;
        int top =160;
        int bottom =560;
        double baseline =0;
        int framecount =0;
        double avg =0;
        int emotionID =0;

        struct point{ //so location will be treated as a single object not seperate numbers
            int x;
            int y;
        };
        point landmarks[5];
        vector<unsigned char> rgbaBuffer;

    public:
        cameraHandling(){
            fd = -1;
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            bitsLeft =0;
            bitCurrent =0;
            frameBuffer.resize(921600);
            rgbaBuffer.resize(921600*4);
            running = true;
            y_h =1;
            y_v =1;
            if(SDL_Init(SDL_INIT_VIDEO)<0){
                cout << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
            }
            window = SDL_CreateWindow("Mimic", 100, 100, 2134, 600, SDL_WINDOW_SHOWN);
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 1280, 720);
            surface = SDL_LoadBMP("proud.bmp");
            if(surface!=0){
                happy = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
                cout << "Success: Asset Loaded!" << endl;

            } else if (surface ==0) {
                cout << "FAILURE" << endl;
            }
            for(int i =0;i<8;i++){
                for(int j =0; j<8;j++){
                    cosTable[i][j] = cos(((2*i+1)*j*M_PI)/16.0);
                }
            }
            for(int i=0;i<5;i++){
                landmarks[i].x =0;
                landmarks[i].y =0;
            }
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
                        // running = false;
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
        return 0;
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


        double cosValn =0, cosValk =0;
        for(int n=0;n<8;n++){
            for(int k=0;k<8;k++){
                    double sum =0.0;
                    for(int u=0;u<8;u++){
                        double cu = 1.0;
                        if(u==0){
                            cu = 0.70710678118;
                        } 
                        for(int v=0;v<8;v++){
                            double cv =1.0;
                            if(v==0){
                                cv = 0.70710678118;
                            } 
                            sum += cu*cv*spatialGrid[u * 8 + v]*cosTable[n][u]*cosTable[k][v];
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

    void extractor(){

        int darkestEyeleft = 40000;
        int darkestEyeright = 40000;
        int offset =0;
        int fini =0;
        int curBrightness =0;
        int midx = 640;
        int midy = 360;
        int darkestMouthleft = 40000;
        int darkestMouthright = 40000;
        int noseDrake = 40000;
        int n =0;
        int facemidx = (landmarks[0].x+landmarks[1].x)/2;
        int facemidy = (landmarks[0].y+landmarks[1].y)/2;

        for(int y = top; y<bottom-4;y++){
            if (y<=midy){
                    for(int x = left+30; x<midx-10; x++){
                        int areaSum =0;

                        for(int cipars = 0; cipars<4;cipars++){
                            for(int cits = 0; cits<4;cits++){
                                n = (y+cipars)*picWidth+(x+cits);
                                areaSum += frameBuffer[n];//eyes left
                            }
                        }
                        if(areaSum<darkestEyeleft && areaSum<5000){
                            darkestEyeleft = areaSum;
                            landmarks[1].x = x+2;
                            landmarks[1].y =y+2;
                        }
                    }
                    for(int x = midx+10; x<right-30; x++){
                        int areaSum =0;

                        for(int cipars = 0; cipars<4;cipars++){
                            for(int cits = 0; cits<4;cits++){//eyes right
                                n = (y+cipars)*picWidth+(x+cits);
                                areaSum += frameBuffer[n];
                            }
                        }
                        if(areaSum<darkestEyeright && areaSum<5000){
                            darkestEyeright = areaSum;
                            landmarks[0].x = x+2;
                            landmarks[0].y =y+2;
                        }
                } 
            } if (y>=midy+40 && y<midy+120){
                for(int x = midx-100; x<midx-40; x++){
                        int areaSum =0;

                        for(int cipars = 0; cipars<4;cipars++){
                            for(int cits = 0; cits<4;cits++){
                                n = (y+cipars)*picWidth+(x+cits);//mouth left
                                areaSum += frameBuffer[n];
                            }
                        }
                        if(areaSum<darkestMouthleft && areaSum<5000){
                            darkestMouthleft = areaSum;
                            landmarks[3].x = x+2;
                            landmarks[3].y =y+2;
                        }
                }
                for(int x = midx+40; x<midx+100; x++){
                        int areaSum =0;

                        for(int cipars = 0; cipars<4;cipars++){
                            for(int cits = 0; cits<4;cits++){
                                n = (y+cipars)*picWidth+(x+cits);
                                areaSum += frameBuffer[n];
                            }
                        }
                        if(areaSum<darkestMouthright && areaSum<5000){
                            darkestMouthright = areaSum;//mouth right
                            landmarks[2].x = x+2;
                            landmarks[2].y =y+2;
                        }
                }
            }
        }


        for(int y = midy+35;y<midy+65;y++){
            for(int x = midx-5;x<midx+5;x++){
                int areaSum =0;

                for(int cipars = 0; cipars<4;cipars++){
                    for(int cits = 0; cits<4;cits++){
                        n = (y+cipars)*picWidth+(x+cits);
                        areaSum += frameBuffer[n];
                    }
                }
                if(areaSum<noseDrake && areaSum<5000){
                    noseDrake = areaSum;//nose
                    landmarks[4].x = x+2;
                    landmarks[4].y =y+2;
                }
            }
        }

        cout << "right Eye: " << landmarks[0].x << " left Eye: " << landmarks[1].x << " right mouth corner: " << landmarks[2].x << " left mouth corner: " << landmarks[3].x << " nose: " << landmarks[4].x <<endl;        

    }

    int emotionDetector(){
        emotionID = 0;
        bool happy = false;
        double edx = landmarks[1].x-landmarks[0].x;
        double edy = landmarks[1].y-landmarks[0].y;
        double ed = (edx*edx)+(edy*edy);
        double eyeDistance = sqrt(ed);

        double mwx = landmarks[3].x-landmarks[2].x;
        double mwy = landmarks[3].y-landmarks[2].y;
        double mw = (mwx*mwx)+(mwy*mwy);
        double mouthWidth = sqrt(mw);
        double smileScore = mouthWidth/eyeDistance;
        cout<< " SMILE SCORE "<<smileScore<<endl;

        if(framecount<100){
            baseline += smileScore;
            framecount++;
        } else{
            avg = baseline/100;
        }

        if(smileScore>avg*1.15){
            happy = true;
        }

        if(happy){
            emotionID =1;
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
                if(!running || done) return 0;
                checkEvents();
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
        updateScreen();
        return 0;
    }

    void checkEvents(){
        SDL_Event mail;
        while (SDL_PollEvent(&mail)){
            if(mail.type == SDL_QUIT){
                done = true;
                running = false;
            }
        }
    }

    void updateScreen(){
        SDL_Rect leftRect = {0, 0, 1067, 600};
        SDL_Rect RightRect = {1067, 0, 1067, 600};

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
        SDL_RenderClear(renderer);
        SDL_Rect roi = {(left * 1067) / 1280, (top * 600) / 720, ((right - left) * 1067) / 1280, ((bottom - top) * 600) / 720};
        for(int i =0; i<921600;i++){
            unsigned char gr = frameBuffer[i];
            rgbaBuffer[i*4+0] = gr;
            rgbaBuffer[i*4+1] = gr;
            rgbaBuffer[i*4+2] = gr;
            rgbaBuffer[i*4+3] = 255;
        }
        SDL_UpdateTexture(texture, NULL, rgbaBuffer.data(), 1280*4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &leftRect);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        for(int j =0;j<5;j++){
            int x = (landmarks[j].x*1067/1280);
            int y = (landmarks[j].y*600/720);
            SDL_Rect dot = {x-2,y-2,4,4};
            SDL_RenderFillRect(renderer, &dot);
        }
        SDL_RenderDrawRect(renderer, &roi);

        if(emotionID == 1){
            SDL_RenderCopy(renderer, happy, NULL, &RightRect);
        }

        SDL_RenderPresent(renderer);


    }

    void resetter(){
        running = !done;
        bitsLeft =0;
        bitCurrent =0;
        precDC[0] =0;
        precDC[1] =0;
        precDC[2] =0;
    }

    bool isRunning(){
        return !done;
    }

    int endStream(){
        if(ioctl(fd, VIDIOC_STREAMOFF, &type) < 0){
            perror("Could not end streaming, VIDIOC_STREAMOFF");
            return 1;
        }
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
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
    while(cam.isRunning()){
        if(cam.captureLoop()!=0) break;
        cam.extractor();
        cam.emotionDetector();
    }
    cam.endStream();


    return 0;
}

// 0x FFD8	SOI	Start of Image	
// 0x FFD9	EOI	End of Image	
// 0x FFDA	SOS	Start of Scan	
// 0x FFDB	DQT	Define Quantization Table
// 0x FFC4  Hufffman table