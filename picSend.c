#include <stdio.h>
#include <errno.h>

#include "highgui.h"
#include "opencv.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>

#define IMG_TRANS_PORT      12088

#define MAX_DATA_BUFFER     100

void *captureProcess(void *arg)
{
    // CvCapture *capture = (CvCapture*)arg;
    CvCapture *capture = 0;
    IplImage *frame = 0;
    int key = 0;
    const char *pImageFileName = "capture.jpg";
    struct sockaddr_in server_addr, client_addr;
    int main_socket;
    int new_socket, addrlen;
    FILE *picture;
    int readsize;
    char send_buffer[15000] = {0};
    char read_buffer[256] = {0};
    int packet = 0;
    char msg_ack[] = "got it!";
    char sizeString[MAX_DATA_BUFFER];
    int size;

    // pCapture = cvCreateCameraCapture(0);

    /* initialize camera */
    capture = cvCaptureFromCAM(0);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 320);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
    // cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, 5);

    if (!capture)
    {
        fprintf(stderr, "Cannot open initialize webcam!\n");
        return (void*)0;
    }

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

    printf("accept socket...\n");
    addrlen = sizeof(struct sockaddr); // important
    new_socket = accept(main_socket, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
    printf("new_socket=%d\n", new_socket);
    if (new_socket < 0)
    {
        printf("accept failed\n");
        return (void*)1;
    }

    printf("accept succ!\n");
    while(1)
    {
        printf("get a frame capture\n");
        /* get a frame */
        frame = cvQueryFrame(capture);
        if (!frame)
        {
            break;
            fprintf(stdout, "ERROR: frame is null\n");
        }

        printf("save a picture\n");
        cvSaveImage(pImageFileName, frame);

        // send image
        picture = fopen(pImageFileName, "rb");
        if (picture == NULL)
        {
            printf("open pic failed\n");
            continue;
        }

        fseek(picture, 0, SEEK_END);
        size = ftell(picture);
        fseek(picture, 0, SEEK_SET);
        printf("picture size: %d kb\n", size / 1000);

        // send picture size
        write(new_socket, (void *)&size, sizeof(int));

        // wait client ack
        // readline(new_socket, (void*)sizeString, DATA_STREAM_LEN);
        // if (strcmp(sizeString, msg_ack))
        // {
        //     printf("client has not ack!\n");
        //     continue;
        // }

        while(!feof(picture))
        {
             readsize = fread(send_buffer, 1, sizeof(send_buffer) - 1, picture);
             int ret = write(new_socket, send_buffer, readsize);
             if (-1 == ret)
             {
                 printf("%m", errno);
                 cvReleaseCapture(&capture);
                 return (void*)2;
             }

             printf("read_size = %d, packet_index = %d\n", readsize, packet);
             packet++;

             bzero(send_buffer, sizeof(send_buffer));
        }
    }

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
