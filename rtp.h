#ifndef RTP_H

#define RTP_H

#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include <jrtplib3/rtpsessionparams.h>

#define DEFAULT_PT        96        /* 默认payload type为96 */
#define DEFAULT_MARK      false     /* 默认MARKER为false */
#define MAX_PAYLOAD_SIZE  1000U     /* 最大RTP payload大小: MTU - IP头 - UDP头 - RTP头 ~= 1400 */

class RTPSender {
public:
    
    RTPSender(double tsunit, uint32_t timestampinc);

    ~RTPSender();

    /**
     * 发送RTP数据
     * 
     * @param data 一个H264 NALU, 不包含起始字节00 00 00 01
     * @param len NALU的长度
     * @return 发送成功则返回0, 否则返回负数
     * 
     * @see https://tools.ietf.org/html/rfc3984
     */
    int send(const uint8_t* data, size_t len);

    /**
     * 添加RTP目的地址
     * 
     * @param addr 点分十进制字符串
     * @param port 端口号
     * @return 添加成功返回0, 否则返回负数
     */
    int addDest(const char *addr, uint16_t port);

    /**
     * 删除RTP目的地址
     * 
     * @param addr 点分十进制字符串
     * @param port 端口号
     * @return 删除成功返回0, 否则返回负数
     */
    int delDest(const char *addr, uint16_t port);

private:
    
    jrtplib::RTPSessionParams sessparams;
    
    jrtplib::RTPSession session;

};

#endif // RTP_H