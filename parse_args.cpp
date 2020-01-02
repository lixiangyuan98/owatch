/**
 * 此文件处理命令行参数
 */
#include <iostream>
#include <getopt.h>
#include <cstdlib>

#include "parse_args.h"

static const char *helpString = "\
usage: owatch [-h] [-i inc] [-p size] [-r rtp_port] [--sync]\n\
              [--rate rate] [-s path] [-t ttl] host port\n\n\
options:\n\
    -h, --help          display help and exit\n\
    -i, --inc inc       timestamp increment of per packet, default 3750\n\
    -p, --payload size  max size of per H264 payload, default 20, means 2^20 Bytes\n\
    -r, --rtp port      the port that client use to transmit RTP packet, default 9000\n\
        --rate rate     sample rate of the input data, default 90000Hz\n\
    -s, --socket path   the absolute path of Unix Domain Socket(UDS), default /tmp/owatch.sock\n\
        --sync          send RTP packet synchronously\n\
    -t, --ttl ttl       seconds to wait before closing a connection, default 30\n\n\
examples:\n\
    owatch -s /tmp/owatch.sock 0.0.0.0 8000     start a server, collecting data from socket\n\n\
Sources and documens are available at: https://github.com/lixiangyuan98/owatch";

static const char *shortOpts = "hi:p:r:s:t:";
static const struct option longOpts[] = {
    {"help", no_argument, NULL, 'h'},
    {"inc", required_argument, NULL, 'i'},
    {"payload", required_argument, NULL, 'p'},
    {"rtp", required_argument, NULL, 'r'},
    {"rate", required_argument, NULL, 0},
    {"socket", required_argument, NULL, 's'},
    {"sync", no_argument, NULL, 1},
    {"ttl", required_argument, NULL, 't'},
};

ArgParser::ArgParser(int argc, char **argv) {
    args.host = "0.0.0.0";
    args.port = 8000;
    args.socketName = "/tmp/owatch.sock";
    args.rtpPort = 9000;
    args.ttl = 30;
    args.sync = false;
    args.sampleRate = 90000;
    args.timestampinc = 3750;
    args.payloadSize = 20;
    parseArgs(argc, argv);
}

void ArgParser::help() {
    std::cout << helpString << std::endl;
}

void ArgParser::parseArgs(int argc, char **argv) {
    
    if (argc < 3) {
        help();
        exit(-1);
    }

    args.host = argv[argc - 2];
    args.port = atoi(argv[argc - 1]);

    int opt = getopt_long(argc, argv, shortOpts, longOpts, NULL);
    while (opt != -1) {
        switch (opt) {
        case 'h':
            help();
            exit(0);

        case 'i':
            args.sampleRate = atoi(optarg);
            break;
        case 'p':
            args.payloadSize = atoi(optarg);
            break;
        
        case 'r':
            args.rtpPort = atoi(optarg);
            break;

        case 's':
            args.socketName = optarg;
            break;
        
        case 't':
            args.ttl = atoi(optarg);
            break;
        
        case 0:
            args.timestampinc = atoi(optarg);
            break;

        case 1:
            args.sync = true;
            break;
        
        default:
            help();
            exit(-1);
        }
        opt = getopt_long(argc, argv, shortOpts, longOpts, NULL);
    }
}

struct Args* ArgParser::getArgs() {
    return &args;
}
