#include <iostream>

#include "rtp.h"
#include "collector.h"
#include "connection.h"
#include "parse_args.h"

UDSCollector *collector;
VideoSender *sender;
struct Args *args;

/**
 * 发送线程的参数
 */
struct SendArg {
    uint8_t *data;
    size_t len;
};

static void onConnect(uint32_t addr) {
    fprintf(stdout, "addr=%s connect\n", inet_ntoa({htonl(addr)}));
    sender->addDest(inet_ntoa({htonl(addr)}), args->rtpPort);
}

static void onLeave(uint32_t addr) {
    fprintf(stdout, "addr=%s leave\n", inet_ntoa({htonl(addr)}));
    sender->delDest(inet_ntoa({htonl(addr)}), args->rtpPort);
}

/**
 * Wrapper function for Connection::serve()
 */
static void *startHeartbeatServer(void *conn) {
    ((Connection*)conn)->serve();
    free(conn);
    return 0;
}

/**
 * Wrapper function for VideoSender::send()
 */
static void *_send(void *arg) {
    int status;
    struct SendArg* sendArg = (struct SendArg*)arg;
    if ((status = sender->send(sendArg->data, sendArg->len)) < 0) {
        fprintf(stderr, "send error: %s\n", jrtplib::RTPGetErrorString(status).c_str());
    }

    free(sendArg->data);
    sendArg->data = NULL;
    free(arg);
    arg = NULL;

    return 0;
}

/* 利用新线程异步发送 */
static void sendAsync(uint8_t *data, int len) {
    pthread_t sendThreadId;
    struct SendArg *arg = (struct SendArg*)malloc(sizeof(struct SendArg));
    arg->data = (uint8_t *)malloc(len - 4);
    arg->len = (size_t)len - 4;
    memcpy(arg->data, data + 4, len - 4);
    pthread_create(&sendThreadId, NULL, _send, arg);
}

/* 同步发送 */
static void sendSync(uint8_t *data, int len) {
    int status;
    if ((status = sender->send(data + 4, len - 4)) < 0) {
        fprintf(stderr, "send error: %s\n", jrtplib::RTPGetErrorString(status).c_str());
    }
}

int main(int argc, char ** argv) {

    ArgParser argParser(argc, argv);
    args = argParser.getArgs();
    
    /* 启动心跳检测线程 */
    Connection *conn = new Connection(args->host, args->port, args->ttl, onConnect, NULL, onLeave);
    pthread_t tid;
    if (pthread_create(&tid, NULL, startHeartbeatServer, conn) < 0) {
        fprintf(stderr, "create heartbeat thread error\n");
        exit(-1);
    }
    
    void (*send)(uint8_t *data, int len);
    if (args->noThread) {
        send = sendSync;
    } else {
        send = sendAsync;
    }
    
    /* 采集数据并发送 */
    collector = new UDSCollector(args->socketName);
    sender = new VideoSender(1.0/args->sampleRate, args->timestampinc);
    uint8_t recvBuff[1 << args->payloadSize];
    int len;
    for (;;) {
        len = collector->collect(recvBuff, 1 << args->payloadSize);
        if (len < 0) {
            fprintf(stderr, "collect error\n");
            continue;
        }
        send(recvBuff, len);
    }

    return 0;
}
