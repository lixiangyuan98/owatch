/**
 * 此文件处理输入数据
 */
#include <iostream>

#include "collector.h"

FileCollector::FileCollector(const char *fn) {
    input.open(fn);
}

size_t FileCollector::collect(uint8_t *data, size_t maxLen) {
    size_t len;
    uint32_t pattern;

    input.read((char*)data, 4);
    /* 检查头4字节是否是NALU起始码 */
    if (input.gcount() != 4 || 
        (pattern = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) != 0x00000001) {
        std::cout << "offset:" << input.tellg() << " len:" << input.gcount() << std::endl;
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