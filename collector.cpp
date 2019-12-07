/**
 * 此文件处理输入数据
 */
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "collector.h"

FileCollector::FileCollector(const char *fn) {
    input.open(fn);
    if (input.fail()) {
        fprintf(stderr, "open file error\n");
        exit(-1);
    }
}

size_t FileCollector::collect(uint8_t *data, size_t maxLen) {
    size_t len;
    uint32_t pattern;

    input.read((char*)data, 4);
    /* 检查头4字节是否是NALU起始码 */
    if (input.gcount() != 4 || 
        (pattern = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) != 0x00000001) {
        return ESTART;
    }
    len = 4;

    while (!input.eof() && len < maxLen) {
        input.read((char*)&data[len], 1);
        len++;

        pattern = pattern << 8 | data[len - 1];
        /* 读到下一NALU起始码, 退回4字节 */
        if (pattern == 0x00000001) {
            len -= 4;
            input.seekg(-4, input.cur);
            break;
        }
    }

    /* 超出最大长度, 退回到本次读取的起始处 */
    if (len >= maxLen) {
        input.seekg(-len, std::ios::cur);
        return EBUFFLEN;
    }

    /* 循环读取文件 */
    if (input.eof()) {
        input.clear();
        input.seekg(0);
    }

    return len;
}

FileCollector::~FileCollector() {
    input.close();
}

UDSCollector::UDSCollector(const char* s) {
    if ((fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "create socket failed\n");
        exit(-1);
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
    strcpy(addr.sun_path, s);
    /* 先unlink()避免bind()失败 */
    unlink(s);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind socket failed\n");
        exit(-1);
    }

}

size_t UDSCollector::collect(uint8_t *data, size_t maxLen) {
    struct sockaddr_un addr;
    socklen_t addrLen = sizeof(addr);
    int len;

    /* 从Unix域套接字中读取一个NALU */
    if ((len = recvfrom(fd, data, maxLen, 0, (struct sockaddr *)&addr, &addrLen)) < 0) {
        return ERDUDS;
    }

    return len;
}

UDSCollector::~UDSCollector() {
    close(fd);
}