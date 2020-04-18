#include <iostream>
#include <queue>
#include <unistd.h>

#include "rtp.h"
#include "collector.h"
#include "connection.h"
#include "parse_args.h"

/* 实时数据发送队列的负载类型 */
struct send_payload_t {
    uint8_t *data;
    size_t len;
};

/* 实时数据采集 */
static UDSCollector *collector;
/* 实时数据RTP发送 */
static RTPSender *sender;

/* 发送队列 */
static std::queue<struct send_payload_t> sendQueue;
static pthread_mutex_t sendQueueMu;
static pthread_cond_t sendQueueCond;

/* 服务用户数 */
static int userCount;
static pthread_mutex_t userCountMu;
static pthread_cond_t userCountCond;

/* 正在发送数据的目的地端口 */
static std::map<uint32_t, uint16_t> sendDataPorts;
static pthread_mutex_t sendDataPortsMu;

/* 正在发送文件的dests */
static std::map<uint32_t, pthread_t> sendFileThreads;
static pthread_mutex_t sendFileTheadsMu;

/* 全局运行参数 */
static struct Args *args;

static void onConnect(uint32_t addr, uint16_t port) {
    port = port == 0 ? args->port : port;
    pthread_mutex_lock(&userCountMu);
    if (sender->addDest(inet_ntoa((struct in_addr){s_addr: htonl(addr)}), port) == 0) {
        fprintf(stdout, "addr=%s:%d connect video\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}), port);
        userCount++;
        pthread_cond_signal(&userCountCond);
    }
    pthread_mutex_unlock(&userCountMu);
}

static void onHeartbeat(uint32_t addr, uint16_t port) {
    fprintf(stdout, "addr=%s:%d heartbeat\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}), port);
}

static void onLeave(uint32_t addr, uint16_t port) {
    port = port == 0 ? args->port : port;
    pthread_mutex_lock(&userCountMu);
    if (sender->delDest(inet_ntoa((struct in_addr){s_addr: htonl(addr)}), port) == 0) {
        fprintf(stdout, "addr=%s:%d leave video\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}), port);
        userCount--;
    }
    pthread_mutex_unlock(&userCountMu);
}

/* Wrapper function for Connection::serve() */
static void *startHeartbeatServer(void *s) {
    ((HeartbeatServer*)s)->serve();
    free(s);
    return 0;
}

/* 异步发送后台线程 */
static void *_send(void *arg) {
    int status;
    struct send_payload_t payload;
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
    struct send_payload_t payload;
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

static void *startClient(void *arg) {
    ((Client*)arg)->start();
    free(arg);
    return 0;
}

struct _send_file_arg_t {
    uint32_t addr;
    uint16_t port;
    const char src[255];
};

/* 发送文件的线程 */
static void *_sendFile(void *arg) {
    RTPSender sender(1.0/args->sampleRate, args->timestampinc);
    uint32_t addr = ((struct _send_file_arg_t *)arg)->addr;
    sender.addDest(inet_ntoa((struct in_addr){s_addr: htonl(addr)}), ((struct _send_file_arg_t *)arg)->port);
    char filename[255];
    strcat(filename, args->directory);
    strcat(filename + strlen(filename), "/");
    strcat(filename + strlen(filename), ((struct _send_file_arg_t *)arg)->src);
    
    FileCollector fc(filename);
    uint8_t buffer[1 << 20];
    int status;
    for (;;) {
        status = fc.collect(buffer, 1 << 20);
        if (status < 0) {
            fprintf(stderr, "collect file error\n");
            break;
        }
        if ((status = sender.send(buffer + 4, status - 4)) < 0) {
            fprintf(stderr, "send error: %s\n", jrtplib::RTPGetErrorString(status).c_str());
        }
        jrtplib::RTPTime::Wait(jrtplib::RTPTime(0, 25000));
    }
}

static void startSendFile(uint32_t addr, uint16_t port, const char *src) {
    pthread_mutex_lock(&sendFileTheadsMu);

    std::map<uint32_t, pthread_t>::iterator iter = sendFileThreads.find(addr);
    if (iter == sendFileThreads.end()) {
        pthread_t tid;
        struct _send_file_arg_t arg = {};
        arg.addr = addr;
        arg.port = port;
        memcpy((void*)arg.src, src, strlen(src));
        if (pthread_create(&tid, NULL, _sendFile, &arg) < 0) {
            fprintf(stderr, "create send file thread error\n");
            return;
        }
        pthread_detach(tid);
        sendFileThreads.insert(std::pair<uint32_t, pthread_t>(addr, tid));
    }

    pthread_mutex_unlock(&sendFileTheadsMu);
}

static void stopSendFile(uint32_t addr) {
    pthread_mutex_lock(&sendFileTheadsMu);

    pthread_t tid;
    std::map<uint32_t, pthread_t>::iterator iter = sendFileThreads.find(addr);
    if (iter != sendFileThreads.end()) {
        tid = iter->second;
        if (pthread_cancel(tid) < 0) {
            fprintf(stdout, "stop send file error\n");
        }
        sendFileThreads.erase(addr);
    }

    pthread_mutex_unlock(&sendFileTheadsMu);
}

/* 接收到指令后的处理函数 */
static void onRecvCommand(Client::command_t *command) {
    fprintf(stdout, "recv command %s %s %s %s\n", command->method, command->dest, command->port, command->src);
    struct in_addr inAddr = {};
    inet_aton(command->dest, &inAddr);
    uint32_t addr = htonl(inAddr.s_addr);
    uint16_t port = atoi(command->port);
    /* 保存目的端口 */
    uint16_t lastPort = 0;
    pthread_mutex_lock(&sendDataPortsMu);
    std::map<uint32_t, uint16_t>::iterator iter = sendDataPorts.find(addr);
    if (iter != sendDataPorts.end()) {
        lastPort = iter->second;
    }
    sendDataPorts.insert(std::pair<uint32_t, uint16_t>(addr, port));
    pthread_mutex_unlock(&sendDataPortsMu);
    
    if (strcmp(command->method, "SEND") == 0) {
        if (command->src == NULL) {
            /* 发送视频 */
            onLeave(addr, lastPort);
            stopSendFile(addr);
            onConnect(addr, port);
        } else {
            /* 发送文件 */
            onLeave(addr, lastPort);
            stopSendFile(addr);
            startSendFile(addr, port, command->src);
        }
    } else if (strcmp(command->method, "STOP") == 0) {
        /* 停止发送 */
        stopSendFile(addr);
        onLeave(addr, lastPort);

        pthread_mutex_lock(&sendDataPortsMu);
        sendDataPorts.erase(addr);
        pthread_mutex_unlock(&sendDataPortsMu);
    }
}

int main(int argc, char **argv) {

    ArgParser argParser(argc, argv);
    args = argParser.getArgs();

    /* 启动客户端程序 */
    sendDataPorts = std::map<uint32_t, uint16_t>();
    pthread_mutex_init(&sendDataPortsMu, NULL);
    sendFileThreads = std::map<uint32_t, pthread_t>();
    pthread_mutex_init(&sendFileTheadsMu, NULL);
    Client *client = new Client(args->host, args->port, 10, args->directory, onRecvCommand);
    pthread_t tid;
    if (pthread_create(&tid, NULL, startClient, client) < 0) {
        fprintf(stderr, "create client thread error\n");
        exit(-1);
    }
    pthread_detach(tid);
    
    void (*send)(uint8_t *data, int len);
    if (args->sync) {
        send = sendSync;
    } else {
        send = sendAsync;
        pthread_mutex_init(&sendQueueMu, NULL);
        pthread_cond_init(&sendQueueCond, NULL);
        /* 启动异步发送线程 */
        pthread_t sendThreadId;
        if (pthread_create(&sendThreadId, NULL, _send, NULL) < 0) {
            fprintf(stderr, "create sync send thread fail\n");
            exit(-1);
        }
        pthread_detach(sendThreadId);
    }
    
    /* 采集数据并发送 */
    collector = new UDSCollector(args->socketName);
    sender = new RTPSender(1.0/args->sampleRate, args->timestampinc);
    uint8_t recvBuff[1 << args->payloadSize];
    int len;
    for (;;) {
        len = collector->collect(recvBuff, 1 << args->payloadSize);
        if (len <= 0) {
            fprintf(stderr, "collect from socket error\n");
            continue;
        }
        pthread_mutex_lock(&userCountMu);
        while (userCount == 0) {
            pthread_cond_wait(&userCountCond, &userCountMu);
        }
        send(recvBuff, len);
        pthread_mutex_unlock(&userCountMu);
    }

    return 0;
}
