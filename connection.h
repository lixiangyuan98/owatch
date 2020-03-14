#ifndef CONNECT_H

#define CONNECT_H

#include <map>
#include <pthread.h>

#define PACKETSIZE     8            /* 心跳包的大小为8 Bytes */

typedef void (*onEvnFunc)(uint32_t addr, uint16_t port);       /* 客户端首次连接/更新/失联时的回调函数类型 */
typedef std::map<uint32_t, timer_t> conns_t;    /* 客户端连接信息容器类型 */

/**
 * 心跳服务端
 * 在master模式下, 作为服务端主动维护客户端的心跳
 */
class HeartbeatServer {
public:

    /**
     * @param addr 点分十进制表示的监听的IP地址
     * @param port 监听端口
     * @param onConnect 客户端连接的回调函数
     * @param onHeartBeat 客户端发送心跳的回调函数
     * @param onLeave 客户端断开的回调函数
     */
    HeartbeatServer(const char* addr, uint16_t port, uint16_t heartbeatFreq, 
               onEvnFunc onConnectFunc, onEvnFunc onHeartbeatFunc, onEvnFunc onLeaveFunc);

    ~HeartbeatServer();

    /**
     * 接收客户端的心跳包
     * 
     * 根据收到的心跳包更新客户端连接信息: 若该连接已存在, 则更新timestamp, 否则创建新连接
     * 对于长时间未收到心跳包的连接, 将其关闭
     */
    void serve();

private:

    /* 接收心跳包的socket描述符 */
    int sock;

    /* 心跳超时断开时间, 单位: 秒 */
    struct itimerspec heartbeatFreq;

    /* 客户端连接信息 */
    conns_t *conns;

    /* 互斥信号量 */
    pthread_mutex_t *connsMu;

    /* 客户端首次连接时调用的回调函数 */
    onEvnFunc onConnectFunc;

    /* 客户端发送心跳包时调用的回调函数 */
    onEvnFunc onHeartbeatFunc;

    /* 客户端断开连接时调用的回调函数 */
    onEvnFunc onLeaveFunc;

    int updateTimer(timer_t tid);

    int setTimer(timer_t *tid, uint32_t addr);
};

/**
 * 命令客户端
 * 在slave模式下, 作为客户端主动连接管理程序, 接受管理程序发来的指令
 */
class Client {
public:

    /* 控制端下发的命令 */
    struct command_t {
        char *method;
        char *dest;
        char *port;
        char *src;
    };
    
    Client(const char* addr, uint16_t port, uint16_t reportFreq, const char* dir, void (*onRecvCommandFunc)(Client::command_t *command));

    ~Client();

    /**
     * 启动新线程, 周期性上报数据
     */
    void start();

private:

    struct sockaddr_in serverAddr;

    struct itimerspec reportFreq;

    const char *dir;

    void (*onRecvCommandFunc)(Client::command_t *command);

    /**
     * 解析收到的命令, 并处理
     */
    void _recv(char *m, size_t len);

    static void *_send(void* arg);
};

#endif // CONNECT_H