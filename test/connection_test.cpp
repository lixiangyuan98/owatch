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

#include "../connection.h"

void onConn(uint32_t addr) {
    fprintf(stdout, "on connect, addr=%s, time=%ld\n", inet_ntoa({htonl(addr)}), time(NULL));
}

void onLeave(uint32_t addr) {
    fprintf(stdout, "on leave, addr=%s, time=%ld\n", inet_ntoa({htonl(addr)}), time(NULL));
}

void onHeartbeat(uint32_t addr) {
    fprintf(stdout, "heartbeat, addr=%s, time=%ld\n", inet_ntoa({htonl(addr)}), time(NULL));
}

int main(int argc, char ** argv) {
    
    if (argc < 4) {
        fprintf(stderr, "argument is required\n");
        exit(-1);
    }

    Connection conn(argv[1], atoi(argv[2]), atoi(argv[3]), onConn, onHeartbeat, onLeave);
    conn.serve();
    return 0;
}