/**
 * 此程序从文件读取H264流并发送到Unix域套接字
 * 
 * Usage:
 * 
 * un_client_test <file path>
 */
#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <stddef.h>
#include <unistd.h>

#include "../collector.h"
#include "../rtp.h"

#define SER_SOCKET "/tmp/un_ser.sock"
#define CLI_SOCKET "/tmp/un_cli.sock"

int main(int argc, char **argv) {

    if (argc < 2) {
        fprintf(stderr, "argument is required\n");
        exit(-1);
    }

    int fd;
    if ((fd = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        std::cout << "create socket failed" << std::endl;
        exit(-1);
    }

    int status;
    /* Unix域套接字客户端不会自动bind socket, 需要手动bind */
    struct sockaddr_un un_cli;
    un_cli.sun_family = AF_LOCAL;
    strcpy(un_cli.sun_path, CLI_SOCKET);
    unlink(CLI_SOCKET);
    if((status = bind(fd, (struct sockaddr *)&un_cli, sizeof(un_cli))) < 0) {
        std::cout << "bind socket failed " << status << std::endl;
        exit(-1);
    }

    struct sockaddr_un un_ser;
    un_ser.sun_family = AF_LOCAL;
    strcpy(un_ser.sun_path, SER_SOCKET);

    FileCollector fc(argv[1]);
    uint8_t *buffer;
    buffer = (uint8_t  *)malloc(1 << 20);

    for (;;) {
        status = fc.collect(buffer, 1 << 20);
        if (status < 0) {
            std::cout << "collect error:" << status << std::endl;
            exit(-1);
        }
        if ((status = sendto(fd, buffer, status, 0, (struct sockaddr *)&un_ser, sizeof(un_ser))) < 0) {
            std::cout << "send error" << std::endl;
        }

        jrtplib::RTPTime::Wait(jrtplib::RTPTime(0, 25000));
    }

    return 0;
}