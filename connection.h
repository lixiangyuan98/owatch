#ifndef CONNECT_H

#define CONNECT_H

#include <map>
#include <mutex>

#define PACKETSIZE     8            /* 心跳包的大小为8 Bytes */

typedef void (*onEvnFunc)(uint32_t addr);       /* 客户端首次连接/更新/失联时的回调函数类型 */
typedef std::map<uint32_t, timer_t> conns_t;    /* 客户端连接信息容器类型 */

class Connection {
public:

    /**
     * @param addr 点分十进制表示的监听的IP地址
     * @param port 监听端口
     * @param onConnect 客户端连接的回调函数
     * @param onHeartBeat 客户端发送心跳的回调函数
     * @param onLeave 客户端断开的回调函数
     */
    Connection(const char* addr, uint16_t port, uint16_t heartbeatFreq, 
               onEvnFunc onConnect, onEvnFunc onHeartbeat, onEvnFunc onLeave);

    ~Connection();

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
    std::mutex *connsMu;

    /* 客户端首次连接时调用的回调函数 */
    onEvnFunc onConnect;

    /* 客户端发送心跳包时调用的回调函数 */
    onEvnFunc onHeartbeat;

    /* 客户端断开连接时调用的回调函数 */
    onEvnFunc onLeave;

    int updateTimer(timer_t tid);

    int setTimer(timer_t *tid, uint32_t addr);
};

#endif // CONNECT_H