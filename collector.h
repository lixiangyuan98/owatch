#ifndef COLLECTOR_H

#define COLLECTOR_H

#include <stddef.h>
#include <fstream>

#define ESTART -1        /* 错误的头部字节 */
#define EBUFFLEN -2      /* 超出缓冲区长度 */
#define EEOF -3          /* 到达文件末尾 */

/**
 * 从文件中获取RTP的payload
 */
class FileCollector {
public:

    FileCollector(const char* fn);

    ~FileCollector();
    
    /**
     * 从文件读取一个NALU单元
     *
     * @param data 存放返回数据的指针
     * @param maxlen 最大可接受的数据长度
     * @return 成功则返回读取的字节数, 否则返回ESTART或者EBUFFLEN
     */
    size_t collect(uint8_t *data, size_t maxlen);

private:
    std::ifstream input;
};

#endif // COLLECTOR_H