/**
 * 此程序从Unix域套接字读取一个NALU并发送RTP包
 * 
 * Usage:
 * 
 * un_server_test <dest ip> <dest port>
 */
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <cstdlib>
#include <cstdio>

#include "../collector.h"
#include "../rtp.h"

#define SER_SOCKET "/tmp/un_ser.sock"

struct SendArg {
    RTPSender *sender;
    uint8_t *data;
    size_t len;
};

/**
 * Wrapper function for send
 */
static void *_send(void *arg) {
    int status;
    struct SendArg* sendArg = (struct SendArg*)arg;
    if ((status = sendArg->sender->send(sendArg->data, sendArg->len)) < 0) {
        fprintf(stderr, "send error: %s\n", jrtplib::RTPGetErrorString(status).c_str());
    }

    free(sendArg->data);
    sendArg->data = NULL;
    free(arg);
    arg = NULL;

    return 0;
}

int main(int argc, char **argv) {
   
    if (argc < 3) {
        fprintf(stderr, "argument is required\n");
        exit(-1);
    }

    UDSCollector uc(SER_SOCKET);
    RTPSender sender(1.0/90000, 3750);
    sender.addDest(argv[1], atoi(argv[2]));

    uint8_t recvBuff[1 << 20];
    int len;
    pthread_t sendThreadId;
    struct SendArg *arg;
    for (;;) {
        len = uc.collect(recvBuff, 1 << 20);
        if (len < 0) {
            fprintf(stdout, "collect error\n");
            continue;
        }

        /* 利用新线程发送 */
        arg = (struct SendArg*)malloc(sizeof(struct SendArg));
        arg->sender = &sender;
        arg->data = (uint8_t *)malloc(len - 4);
        arg->len = (size_t)len - 4;
        memcpy(arg->data, recvBuff + 4, len - 4);
        pthread_create(&sendThreadId, NULL, _send, arg);
    }

    return 0;
}