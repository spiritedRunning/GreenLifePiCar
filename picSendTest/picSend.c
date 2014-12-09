#include <stdio.h>
#include <errno.h>

//#include <opencv2/opencv.hpp>
//#include "highgui.h"
//#include "opencv.hpp"
//#include <cv.h>
#include <highgui.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>

#define IMG_TRANS_PORT      12088

#define MAX_DATA_BUFFER     100
#define DATA_STREAM_LEN     20 

static struct itimerval oldtv;
struct sockaddr_in server_addr, client_addr;
int main_socket;
int new_socket, addrlen;
FILE *picture;
int readsize;
char send_buffer[15000] = {0};
char read_buffer[256] = {0};
int packet = 0;
char msg_ack[] = "got it!";
int size;
int failtime = 0;

struct itimerval itv;

int getFrameOpr();

void set_timer()
{
    printf("start timer..\n");

    itv.it_interval.tv_sec = 2;
    itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    setitimer(ITIMER_REAL, &itv, &oldtv);
}

void stop_timer()
{
    printf("stop timer..\n");

    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    setitimer(ITIMER_REAL, &itv, &oldtv);
}



void signal_handler(int signo)
{
    if (getFrameOpr())
    {
        stop_timer();
    }

}

int getFrameOpr()
{
    CvCapture *capture = 0;
    IplImage *frame = 0;
    int key = 0;
    const char *pImageFileName = "capture.jpg";
    
    // CvCapture *capture = (CvCapture*)arg;

    // pCapture = cvCreateCameraCapture(0);

    /* initialize camera */
    capture = cvCaptureFromCAM(0);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 320);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
    // cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, 5);

    if (!capture)
    {
        fprintf(stderr, "Cannot open initialize webcam!\n");
        return 0;
    }

    while(1)
    {
        printf("get a frame capture\n");
        /* get a frame */
        frame = cvQueryFrame(capture);
        if (!frame)
        {
            printf("ERROR: frame is null\n");
            break;
        }

        printf("save a picture\n");
        cvSaveImage(pImageFileName, frame);

        // send image
        picture = fopen(pImageFileName, "rb");
        if (picture == NULL)
        {
            printf("open pic failed, sleep 3s.\n");
            sleep(3);
            continue;
        }

        fseek(picture, 0, SEEK_END);
        size = ftell(picture);
        fseek(picture, 0, SEEK_SET);

        // send picture size
        printf("picture size: %d \n", size);
        size = htonl(size);
        printf("picture size: %d \n", size);
        write(new_socket, (void *)&size, sizeof(int));

        while(!feof(picture))
        {
             readsize = fread(send_buffer, 1, sizeof(send_buffer) - 1, picture);
             int ret = write(new_socket, send_buffer, readsize);
             if (-1 == ret)
             {
                 printf("%m", errno);
                 cvReleaseCapture(&capture);
                 fclose(picture);
                 return 2;
             }

             printf("read_size = %d, packet_index = %d\n", readsize, packet);
             packet++;

             memset(send_buffer, 0, sizeof(send_buffer));
        }

        // wait client ack
        char ackString[MAX_DATA_BUFFER] = "";
        read(new_socket, (void*)ackString, DATA_STREAM_LEN);
        while (strcmp(ackString, msg_ack))
        {
            printf("ack; %s\n", ackString);
            sleep(3);
            failtime++;
            if (failtime > 10)
            {
                cvReleaseCapture(&capture);
                printf("stop sending, client is not prepared!\n");
                return 3;
            }
        }
    }
 
    printf("return...");
    return 0;
}

void *captureProcess(void *arg)
{

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(IMG_TRANS_PORT);

    if ((main_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("capture socket init failed\n");
        return (void*)1;
    }

    // set reuse
    const int on = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(main_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("capture bind failed\n");
        return (void*)1;
    }

    listen(main_socket, 10);

    printf("waiting accept socket...\n");
    addrlen = sizeof(struct sockaddr); // important
    new_socket = accept(main_socket, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
    printf("accept_socket = %d\n", new_socket);
    if (new_socket < 0)
    {
        printf("accept failed\n");
        return (void*)1;
    }

    //set_timer();
    //signal(SIGALRM, signal_handler);
    getFrameOpr();

}


int main (void)
{
    int ret;
    pthread_t ntidc;
    void *tret;


    // pCapture = cvCreateCameraCapture(0);
    ret = pthread_create(&ntidc, NULL, captureProcess, NULL);
    if (ret != 0)
    {
        printf("capture thread start failed!\n");
        return -1;
    }

    ret = pthread_join(ntidc, &tret);
    if (ret != 0)
    {
        printf("can't join captureProcess!\n");
        // cvReleaseCapture(&pCapture);
        return -1;
    }
    printf("captureProcess exit code: %d\n", tret);
    
    return 0;

}
