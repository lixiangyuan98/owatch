/** 
 * 此文件维护的连接与服务端或客户端的连接 
 */
#include <arpa/inet.h>
#include <set>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <dirent.h>

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
        fprintf(stderr, "bind socket error\n");
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
        notifyArg->onLeave(notifyArg->addr, 0);
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
        if ((len = recvfrom(sock, buff, PACKETSIZE, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) <= 0) {
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
                onHeartbeatFunc(addr, 0);
            }
        } else {
            /* 新增定时器 */
            timer_t tid;
            setTimer(&tid, addr);
            conns->insert(std::pair<uint32_t, timer_t>(addr, tid));
            if (onConnectFunc) {
                onConnectFunc(addr, 0);
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

Client::Client(const char* addr, uint16_t port, uint16_t reportFreq, const char* dir, void (*onRecvCommandFunc)(Client::command_t *command)) {
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(addr);
    serverAddr.sin_port = htons(port);

    struct timespec ts = {
        tv_sec: reportFreq
    };
    struct itimerspec its = {
        it_interval: {},
        it_value: ts,
    };
    this->reportFreq = its;
    this->dir = dir;
    this->onRecvCommandFunc = onRecvCommandFunc;

    signal(SIGPIPE, SIG_IGN);
}

struct _send_arg_t {
    int sock;
    const char *dir;
};

/* 周期上报数据 */
void *Client::_send(void *arg) {
    ssize_t len;
    for (;;) {
        /* 读取存储视频文件的目录 */
        DIR *dir = opendir(((struct _send_arg_t*)arg)->dir);
        if (dir == NULL) {
            fprintf(stderr, "read directory %s error\n", ((struct _send_arg_t*)arg)->dir);
            return 0;
        }
        struct dirent *direntry;
        char data[4096] = "active\r\n";
        while ((direntry = readdir(dir)) != NULL)
        {
            if (strcmp(direntry->d_name, ".") == 0) {
                continue;
            }
            if (strcmp(direntry->d_name, "..") == 0) {
                continue;
            }
            strcat(data, direntry->d_name);
            strcat(data, "\r\n");
        }
        strcat(data, "\r\n");
        if ((len = send(((struct _send_arg_t*)arg)->sock, data, strlen(data), 0)) <= 0) {
            fprintf(stderr, "send report error\n");
            break;
        }
        sleep(10);
    }
    return 0;
}

void Client::start() {
    int sock;
    ssize_t len;
    char buff[512];
    for (;;) {
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            fprintf(stderr, "create socket error\n");
            continue;
        }
        if(connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
            fprintf(stderr, "connect remote server error\n");
            /* 每30秒重试连接 */
            sleep(30);
            continue;
        }
        /* 创建定时上报线程 */
        pthread_t tid;
        struct _send_arg_t arg = {};
        arg.sock = sock;
        arg.dir = dir;
        if (pthread_create(&tid, NULL, _send, &arg) < 0) {
            fprintf(stderr, "create send report thread error\n");
        } else {
            pthread_detach(tid);
            for (;;) {
                if ((len = recv(sock, buff, 512, 0)) <= 0) {
                    fprintf(stderr, "connection closed by remote\n");
                    break;
                }
                _recv(buff, len - 4); /* 去掉"\r\n" */
                // _recv(buff, len);
            }
        }
        if (shutdown(sock, SHUT_RDWR) < 0) {
            fprintf(stderr, "close connection error\n");
        }
    }
}

void Client::_recv(char *m, size_t len) {
    struct Client::command_t *command = (struct Client::command_t*)malloc(sizeof(struct Client::command_t));
    size_t s = 0;
    size_t i;
    char *t;

    for (i = s; i < len; i++) {
        if (m[i] == ' ') {
            m[i] = '\0';
            break;
        }
    }
    t = (char *)malloc(sizeof(char) * (i - s + 1));
    strncpy(t, m + s, i - s + 1);
    command->method = t;

    s = i + 1;
    for (i = s; i < len; i++) {
        if(m[i] == ' ') {
            m[i] = '\0';
            break;
        }
    }
    t = (char *)malloc(sizeof(char) * (i - s + 1));
    strncpy(t, m + s, i - s + 1);
    command->dest = t;

    s = i + 1;
    for (i = s; i < len; i++) {
        if(m[i] == ' ') {
            m[i] = '\0';
            break;
        }
    }
    t = (char *)malloc(sizeof(char) * (i - s + 1));
    strncpy(t, m + s, i - s + 1);
    command->port = t;

    if (i < len) {
        s = i + 1;
        t = (char *)malloc(sizeof(char) * (len - s + 1));
        strncpy(t, m + s, len - s);
        strncpy(t + len - s + 1, "\0", 1);
        command->src = t;
    } else {
        command->src = NULL;
    }

    onRecvCommandFunc(command);
    free(command->method);
    free(command->dest);
    free(command->port);
    free(command->src);
    free(command);
}