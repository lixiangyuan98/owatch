/** 
 * 此文件维护客户端的连接信息 
 */
#include <arpa/inet.h>
#include <set>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <errno.h>

#include "connection.h"

struct notify_arg_t {
    uint32_t addr;
    conns_t *conns;
    pthread_mutex_t *connsMu;
    onEvnFunc onLeave;
};

HeartbeatServer::HeartbeatServer(const char* addr, uint16_t port, uint16_t heartbeatFreq, 
                       onEvnFunc onConnectFunc, onEvnFunc onHeartbeatFunc, onEvnFunc onLeaveFunc) {

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "create socket error\n");
        exit(-1);
    }
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(addr);
    serverAddr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "create socket error\n");
        exit(-1);
    }

    struct timespec ts = {
        tv_sec: heartbeatFreq
    };
    struct itimerspec its = {
        it_interval: {},
        it_value: ts,
    };
    this->heartbeatFreq = its;
    this->conns = new conns_t();
    this->connsMu = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(this->connsMu, NULL);
    this->onConnectFunc = onConnectFunc;
    this->onHeartbeatFunc = onHeartbeatFunc;
    this->onLeaveFunc = onLeaveFunc;
}

/** 
 * 定时器超时调用
 */
static void notify(union sigval arg) {

    notify_arg_t *notifyArg = (notify_arg_t*)arg.sival_ptr;

    pthread_mutex_lock(notifyArg->connsMu);

    if (timer_delete(notifyArg->conns->at(notifyArg->addr)) < 0) {
        fprintf(stderr, "delete timer error\n");
    }
    notifyArg->conns->erase(notifyArg->addr);

    if (notifyArg->onLeave) {
        notifyArg->onLeave(notifyArg->addr);
    }

    pthread_mutex_unlock(notifyArg->connsMu);
    free(notifyArg);
}

int HeartbeatServer::updateTimer(timer_t tid) {
    
    if (timer_settime(tid, 0, &heartbeatFreq, NULL) < 0) {
        fprintf(stderr, "update timer error: %d\n", errno);
        return -1;
    }
    return 0;
}

int HeartbeatServer::setTimer(timer_t *tid, uint32_t addr) {
    
    struct sigevent sev = {};
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = notify;
    struct notify_arg_t *notifyArg = (struct notify_arg_t*)malloc(sizeof(struct notify_arg_t));
    notifyArg->addr = addr;
    notifyArg->conns = conns;
    notifyArg->connsMu = connsMu;
    notifyArg->onLeave = onLeaveFunc;
    sev.sigev_value.sival_ptr = notifyArg;

    if (timer_create(CLOCK_REALTIME, &sev, tid) < 0) {
        fprintf(stderr, "create timer error: %d\n", errno);
        return -1;
    }

    return updateTimer(*tid);
}

void HeartbeatServer::serve() {
    
    uint8_t buff[PACKETSIZE];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    ssize_t len;
    uint32_t addr;

    for (;;) {
        if ((len = recvfrom(sock, buff, PACKETSIZE, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) < 0) {
            fprintf(stderr, "read from socket error");
            continue;
        }
        addr = ntohl(clientAddr.sin_addr.s_addr);
        
        pthread_mutex_lock(connsMu);

        conns_t::iterator iter = conns->find(addr);
        if (iter != conns->end()) {
            /* 更新定时器 */
            updateTimer(iter->second);
            if (onHeartbeatFunc) {
                onHeartbeatFunc(addr);
            }
        } else {
            /* 新增定时器 */
            timer_t tid;
            setTimer(&tid, addr);
            conns->insert(std::pair<uint32_t, timer_t>(addr, tid));
            if (onConnectFunc) {
                onConnectFunc(addr);
            }
        }

        pthread_mutex_unlock(connsMu);
    }
}

HeartbeatServer::~HeartbeatServer() {
    delete this->conns;
    pthread_mutex_destroy(this->connsMu);
    delete this->connsMu;
}