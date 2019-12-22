/**
 * 此程序从文件读取H264流并发送RTP包
 * 
 * Usage:
 * file_test <file path> <dest ip> <dest port> 
 */
#include <iostream>

#include "../rtp.h"
#include "../collector.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        exit(-1);
    }

    VideoSender sender(1.0/90000, 3750);
    sender.addDest(argv[2], atoi(argv[3]));

    FileCollector fc(argv[1]);
    uint8_t *buffer;
    buffer = (uint8_t  *)malloc(1 << 20);
    int status;
    for (;;) {
        status = fc.collect(buffer, 1 << 20);
        if (status < 0) {
            std::cout << "collect error:" << status << std::endl;
            break;
        }

        status = sender.send(buffer + 4, status - 4);
        if (status < 0) {
            std::cout << "send error:" << jrtplib::RTPGetErrorString(status) << std::endl;
            break;
        }

        /* NALU发送间隔
           需根据情况调整, 建议略小于帧时间差 */
        jrtplib::RTPTime::Wait(jrtplib::RTPTime(0, 25000));
    }

    return 0;
}
