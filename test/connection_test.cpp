/**
 * 此程序测试维护客户端连接的逻辑
 * 
 * Usage:
 * 
 * connection_test <listening host> <listening port> <hearbeat timeout>
 */
#include <iostream>
#include <time.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <cstdio>

#include "../connection.h"

static void onConnect(uint32_t addr) {
    fprintf(stdout, "on connect, addr=%s, time=%ld\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}), time(NULL));
}

static void onLeave(uint32_t addr) {
    fprintf(stdout, "on leave, addr=%s, time=%ld\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}), time(NULL));
}

static void onHeartbeat(uint32_t addr) {
    fprintf(stdout, "heartbeat, addr=%s, time=%ld\n", inet_ntoa((struct in_addr){s_addr: htonl(addr)}), time(NULL));
}

int main(int argc, char ** argv) {
    
    if (argc < 4) {
        fprintf(stderr, "argument is required\n");
        exit(-1);
    }

    Connection conn(argv[1], atoi(argv[2]), atoi(argv[3]), onConnect, onHeartbeat, onLeave);
    conn.serve();
    return 0;
}