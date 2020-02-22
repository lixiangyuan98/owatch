#include <iostream>
#include <queue>
#include <unistd.h>

#include "rtp.h"
#include "collector.h"
#include "connection.h"
#include "parse_args.h"

/**
 * 发送队列的数据类型
 */
struct SendPayload
{
    uint8_t *data;
    size_t len;
};

/* 数据采集 */
static UDSCollector *collector;
/* RTP发送 */
static RTPSender *sender;
/* 发送队列的互斥信号量 */
static pthread_mutex_t sendQueueMu;
/* 发送队列的条件信号量 */
static pthread_cond_t sendQueueCond;
/* 发送队列 */
static std::queue<struct SendPayload> sendQueue;
/* 全局运行参数 */
static struct Args *args;

static void onConnect(uint32_t addr) {
    fprintf(stdout, "addr=%s connect\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}));
    sender->addDest(inet_ntoa((struct in_addr){s_addr: htonl(addr)}), args->rtpPort);
}

static void onHeartbeat(uint32_t addr) {
    fprintf(stdout, "addr=%s heartbeat\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}));
}

static void onLeave(uint32_t addr) {
    fprintf(stdout, "addr=%s leave\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}));
    sender->delDest(inet_ntoa((struct in_addr){s_addr: htonl(addr)}), args->rtpPort);
}

/* Wrapper function for Connection::serve() */
static void *startHeartbeatServer(void *conn) {
    ((Connection*)conn)->serve();
    free(conn);
    return 0;
}

/* 异步发送后台线程 */
static void *_send(void *arg) {
    int status;
    struct SendPayload payload;
    for (;;) {
        pthread_mutex_lock(&sendQueueMu);
        while(sendQueue.empty()) {
            pthread_cond_wait(&sendQueueCond, &sendQueueMu);
        }
        pthread_mutex_unlock(&sendQueueMu);

        payload = sendQueue.front();
        if ((status = sender->send(payload.data, payload.len)) < 0) {
        fprintf(stderr, "send error: %s\n", jrtplib::RTPGetErrorString(status).c_str());
        }
        free(payload.data);
        payload.data = NULL;

        pthread_mutex_lock(&sendQueueMu);
        sendQueue.pop();
        pthread_mutex_unlock(&sendQueueMu);
    }

    return 0;
}

/* 异步发送 */
static void sendAsync(uint8_t *data, int len) {
    struct SendPayload payload;
    payload.data = (uint8_t *)malloc(len - 4);
    payload.len = (size_t)len - 4;
    memcpy(payload.data, data + 4, len - 4);

    /* 加入发送队列 */
    pthread_mutex_lock(&sendQueueMu);
    sendQueue.push(payload);
    pthread_cond_signal(&sendQueueCond);
    pthread_mutex_unlock(&sendQueueMu);
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
    Connection *conn = new Connection(args->host, args->port, args->ttl, onConnect, onHeartbeat, onLeave);
    pthread_t tid;
    if (pthread_create(&tid, NULL, startHeartbeatServer, conn) < 0) {
        fprintf(stderr, "create heartbeat thread error\n");
        exit(-1);
    }
    
    void (*send)(uint8_t *data, int len);
    if (args->sync) {
        send = sendSync;
    } else {
        send = sendAsync;
        pthread_mutex_init(&sendQueueMu, NULL);
        pthread_cond_init(&sendQueueCond, NULL);
        /* 启动异步发送线程 */
        pthread_t sendThreadId;
        pthread_create(&sendThreadId, NULL, _send, NULL);
    }
    
    /* 采集数据并发送 */
    collector = new UDSCollector(args->socketName);
    sender = new RTPSender(1.0/args->sampleRate, args->timestampinc);
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
