#include <wiringPi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>

#include "cv.h"
#include "highgui.h"


#define BINDDING_PORT       5888
#define MAX_DATA_BUFFER     100
#define DATA_STREAM_LEN     10
#define IMG_TRANS_PORT      2088
#define IMG_SERVER_ADDR     "192.168.43.1"

#define GPIO_TRIGGER        4
#define GPIO_ECHO           5

#define GPIO_FIR1           6
#define GPIO_FIR2           7
#define GPIO_SEC1           2
#define GPIO_SEC2           3

#define CMD_FORWARD         "0x55"
#define CMD_BACKWARD        "0x56"
#define CMD_LEFT            "0x57"
#define CMD_RIGHT           "0x58"
#define CMD_STOP            "0x59"

int isNeedstop = 0;

ssize_t readline(int fd, void *ptr, size_t maxlen)
{
    ssize_t n, readBytes;
    char c, *p;

    p = (char *)ptr;
    for(n = 1; n < maxlen; n++)
    {
        TRY_AGAIN:
        if ((readBytes = read(fd, &c, 1)) == 1)
        {
            *p++ = c;
            if (c == '\n')   // readover
                break;
        }
        else if (0 == readBytes)
        {
            if (1 == n)   // EOF no data read
                return 0;
            else
                break;
        }
        else
        {
            if (errno == EINTR)
                goto TRY_AGAIN;

            return -1;   // unknown error
        }
    }
    *p = 0;

    return n;
}

void actionForward()
{
    isNeedstop = 1;

    digitalWrite(GPIO_FIR1, 0);
    digitalWrite(GPIO_FIR2, 1);
    digitalWrite(GPIO_SEC1, 0);
    digitalWrite(GPIO_SEC2, 1);
}

void actionBackward()
{
    isNeedstop = 0;

    digitalWrite(GPIO_FIR1, 1);
    digitalWrite(GPIO_FIR2, 0);
    digitalWrite(GPIO_SEC1, 1);
    digitalWrite(GPIO_SEC2, 0);
}

void actionLeft()
{
    isNeedstop = 0;

    digitalWrite(GPIO_FIR1, 0);
    digitalWrite(GPIO_FIR2, 0);
    digitalWrite(GPIO_SEC1, 0);
    digitalWrite(GPIO_SEC2, 1);
}

void actionRight()
{
    isNeedstop = 0;

    digitalWrite(GPIO_FIR1, 0);
    digitalWrite(GPIO_FIR2, 1);
    digitalWrite(GPIO_SEC1, 0);
    digitalWrite(GPIO_SEC2, 0);
}

void actionStop()
{
    // isNeedstop = 0;

    digitalWrite(GPIO_FIR1, 0);
    digitalWrite(GPIO_FIR2, 0);
    digitalWrite(GPIO_SEC1, 0);
    digitalWrite(GPIO_SEC2, 0);
}

void handleMotors(char *cmd)
{

    if (0 == strcmp(cmd, CMD_FORWARD))
    {
        actionForward(); 
        printf("recive forward command\n");
    }
    else if (0 == strcmp(cmd, CMD_BACKWARD))
    {
        actionBackward();
        printf("recive backward command\n");
    }
    else if (0 == strcmp(cmd, CMD_LEFT))
    {
        actionLeft();
        printf("recive left command\n");
    }
    else if (0 == strcmp(cmd, CMD_RIGHT))
    {
        actionRight();
        printf("recive right command\n");
    }
    else if (0 == strcmp(cmd, CMD_STOP))
    {
        actionStop();
        printf("recive stop command\n");
    }

    return;
}

void *motor_fn(void *arg)
{
    char *dataString = (char *)arg;
    printf("new motor thread!\n");
    handleMotors(dataString);

    return ((void*)0);
}


void *startService(void *arg)
{
    int fd;
    int address_len;
    struct sockaddr_in address;
    fd_set fdset;
    int err;
    pthread_t ntid;

    struct sockaddr_in client_address;
    socklen_t len;
    int client_sockfd;
    char *data;
    static char zx[MAX_DATA_BUFFER];
    char dataString[MAX_DATA_BUFFER];
    struct timeval timeout;
    

    // build socket
    fd = socket(AF_INET, SOCK_STREAM, 0);

    // bind port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(BINDDING_PORT);
    address_len = sizeof(address);
    bind(fd, (struct sockaddr*)&address, address_len);

    // starting listening
    listen(fd, 64);

    printf("waiting control command...");
    while (1)
    {
        // set timeout
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // set fdset
        FD_ZERO(&fdset);
        FD_CLR(fd, &fdset);
        FD_SET(fd, &fdset);
        if ((select(fd + 1, &fdset, NULL, NULL, &timeout)) < 0)
        {
            printf("select failed\n");
            fflush(stdout);
        }

        if (FD_ISSET(fd, &fdset))
        {
            // accept peer connection
            len = sizeof(client_address);
            client_sockfd = accept(fd, (struct sockaddr*)&client_address, &len);

            // recive data
            bzero((void*)dataString, len);
            readline(client_sockfd, (void*)dataString, DATA_STREAM_LEN);
            printf("Server read context: %s\n", dataString);

            // contol motor
            err = pthread_create(&ntid, NULL, motor_fn, (void*)dataString);
            if (err != 0)
            {
                printf("can't create thread: %s\n", strerror(err));
                return (void*)-1;
            }
            // handleMotors(dataString);

            close(client_sockfd);
            printf("==keep waiting==\n");
            fflush(stdout);
        }
        else
        {
            printf(".");
            fflush(stdout);
        }
    }
    return (void*)0;
}

void *detectDistance(void *arg)
{
    struct timeval start;
    struct timeval end;
    double difftime;
    double distance;
    struct timespec interval = {0};
    struct timespec reqtime = {0};
    struct timespec resttime = {0};

    interval.tv_nsec = 500000000;
    reqtime.tv_nsec = 10000;
    resttime.tv_nsec = 500000000;

#define SAFE_DISTANCE 45.0

    printf("start detect\n\n");

    // set pins as output and input
    pinMode(GPIO_TRIGGER, OUTPUT);
    pinMode(GPIO_ECHO, INPUT);

    // set trigger to false
    digitalWrite(GPIO_TRIGGER, 0);

    // allow module to settle
    nanosleep(&interval, NULL);

    while (1)
    {
        // send 10us pulse to trigger
        digitalWrite(GPIO_TRIGGER, 1);
        nanosleep(&reqtime, NULL);
        digitalWrite(GPIO_TRIGGER, 0);
        
        while (1)
        {
            if (1 == digitalRead(GPIO_ECHO))
            {
                // printf("record the start time\n");
                gettimeofday(&start, NULL);
                break;
            }
           
        }
        
        while (1)
        {
            if (0 == digitalRead(GPIO_ECHO))
            {
                // printf("record the end time\n");
                gettimeofday(&end, NULL);
                break;
            }
        }
        
        printf("start time : %d sec, %ld usec. end time : %d sec, %ld usec.\n", start.tv_sec, start.tv_usec, end.tv_sec, end.tv_usec);
        
        // just care usec is ok
        difftime = end.tv_usec - start.tv_usec;
        printf("difftime: %f us\n", difftime);

        // Distance pulse travelled in that time is time multiplied by the speed of sound(cm)
        distance = difftime / 1000000 * 34000 / 2;

        printf("Distance : %.2f cm\n\n", distance);

        // if distance less than 8cm that can stop
        if (isNeedstop && (distance <= SAFE_DISTANCE))
        {
            printf("--------------warning!!!!! stop!!!!-------------\n");
            actionStop();
        }
        
        // have a rest
        nanosleep(&resttime, NULL);
    }

    return (void*)0;
}

void *captureProcess(void *arg)
{
    CvCapture *capture = (CvCapture*)arg;
    // CvCapture *capture = 0;
    IplImage *frame = 0;
    int key = 0;
    const char *pImageFileName = "capture.jpg";
    int server_sockfd;
    int new_socket, addrlen;
    struct sockaddr_in server, client;
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
    // capture = cvCaptureFromCAM(-1);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 320);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 240);
    // cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, 5);

    if (!capture)
    {
        fprintf(stderr, "Cannot open initialize webcam!\n");
        return (void*)0;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(IMG_TRANS_PORT);

    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("capture socket init failed\n");
        return (void*)1;
    }

    if (bind(server_sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("capture bind failed\n");
        return (void*)1;
    }

    listen(server_sockfd, 10);

    printf("accept socket...\n");
    new_socket = accept(server_sockfd, (struct sockaddr*)&client, (socklen_t*)&addrlen);
    if (new_socket < 0)
    {
        printf("accept failed\n");
        return (void*)1;
    }

    while(1)
    {
        /* get a frame */
        frame = cvQueryFrame(capture);
        if (!frame)
        {
            break;
            fprintf(stdout, "ERROR: frame is null\n");
        }

        printf("save a picture");
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
        printf("picture size: %d\n", size);

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
             write(new_socket, send_buffer, readsize);

             printf("read_size=%d, packet_index=%d\n", readsize, packet);
             packet++;

             bzero(send_buffer, sizeof(send_buffer));
        }
    }

}

void initwiringPi()
{
    int i;

    if (wiringPiSetup() == -1)
    {
        exit(1);
    }

    // set pin 0-8 output
    for (i = 0; i < 8; i++)
    {
        pinMode(i, OUTPUT);
    }
}

int main (void)
{
    int ret;
    pthread_t ntids, ntidd, ntidc;
    void *tret;
    CvCapture* pCapture;

    printf("Created by Zachery Liu!\n\n");

    initwiringPi();

    //ret = pthread_create(&ntidd, NULL, detectDistance, NULL);
    //if (ret != 0)
    //{
    //    printf("detectDistance thread start failed!\n");
    //    return -1;
    //}
    ret = pthread_create(&ntids, NULL, startService, NULL);
    if (ret != 0)
    {
        printf("startService thread start failed!\n");
        return -1;
    }

    pCapture = cvCreateCameraCapture(0);
    ret = pthread_create(&ntidc, NULL, captureProcess, NULL);
    if (ret != 0)
    {
        printf("capture thread start failed!\n");
        return -1;
    }

    /* wait for threads */
    //ret = pthread_join(ntidd, &tret);
    //if (ret != 0)
    //{
    //    printf("can't join detectdistance!\n");
    //    return -1;
    //}
    //printf("detectDistance exit code: %d\n", tret);

    ret = pthread_join(ntids, &tret);
    if (ret != 0)
    {
        printf("can't join startservice!\n");
        return -1;
    }
    printf("startService exit code: %d\n", tret);

    ret = pthread_join(ntidc, &tret);
    if (ret != 0)
    {
        printf("can't join captureProcess!\n");
        cvReleaseCapture(&pCapture);
        return -1;
    }
    printf("captureProcess exit code: %d\n", tret);
    
    return 0;

}
