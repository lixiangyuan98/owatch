#ifndef PARSE_ARGS_H

#define PARSE_ARGS_H

struct Args {

    /* 监听心跳包的IP地址 */
    const char *host;

    /* 监听心跳包的端口 */
    uint16_t port;

    /* 读取数据的Unix Domain Socket的绝对路径*/
    const char *socketName;

    /* 发送RTP包的目的端口 */
    uint16_t rtpPort;

    /* 关闭客户端连接的超时时间 */
    uint16_t ttl;

    /* 同步发送 */
    bool sync;

    /* 数据的采样率 */
    uint32_t sampleRate;

    /* 每一帧增加的时间戳数量 */
    uint32_t timestampinc;

    /* H264帧的最大长度 */
    uint32_t payloadSize;
};

class ArgParser {
public:

    ArgParser(int argc, char **argv);

    struct Args *getArgs();

private:

    struct Args args;

    void parseArgs(int argc, char **argv);
    
    void help();
};


#endif // PARSE_ARGS_H