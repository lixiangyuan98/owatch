/**
 * 此文件处理RTP的封包、发送
 */
#include <iostream>
#include <arpa/inet.h>

#include <jrtplib3/rtptimeutilities.h>
#include <jrtplib3/rtperrors.h>
#include <jrtplib3/rtppacket.h>

#include "rtp.h"

VideoSender::VideoSender(double tsunit, uint32_t timestampinc) {
	
    sessparams.SetOwnTimestampUnit(tsunit);

    if (session.Create(sessparams, (jrtplib::RTPUDPv4TransmissionParams*)NULL) < 0) {
        fprintf(stderr, "create sess failed\n");
		exit(-1);
    }

    session.SetDefaultPayloadType(DEFAULT_PT);
	session.SetDefaultMark(DEFAULT_MARK);
	session.SetDefaultTimestampIncrement(timestampinc);
	/* 最大包长为 payload长度 + 12 Bytes包头 */
	session.SetMaximumPacketSize(MAX_PAYLOAD_SIZE + 12);
}

int VideoSender::send(const uint8_t* data, size_t len) {

	int status = 0;

	if (len <= MAX_PAYLOAD_SIZE) {
		status = session.SendPacket((void *)data, len);
	} else {
		/* 超出MAX_PAYLOAD_SIZE, 需要分片, 应从第2个字节开始拆分 */
		size_t start = 1;
		size_t fuLen = 0;

		/* type=28表示FU-A */
		uint8_t fuIdc = (data[0] & 0xE0) | 28;
		uint8_t naluType = data[0] & 0x1F;
		uint8_t fuStart = 0x80 | naluType;
		uint8_t fuEnd = 0x40 | naluType;
		uint8_t fu[MAX_PAYLOAD_SIZE];

		fu[0] = fuIdc;
		while (start < len) {
			if (start == 1) {
				fu[1] = fuStart;
				fuLen = MAX_PAYLOAD_SIZE - 2;
			} else if (start + MAX_PAYLOAD_SIZE - 2 >= len) {
				fu[1] = fuEnd;
				fuLen = len - start;
				/* 发送完最后一个FU后主动增加timestamp */
				session.IncrementTimestampDefault();
			} else {
				fu[1] = naluType;
				fuLen = MAX_PAYLOAD_SIZE - 2;
			}
			memcpy(fu + 2, data + start, fuLen);

			/* 每个FU的timestamp应和NALU的保持一直 因此每次仅增加0 */
			status = session.SendPacket((void *)fu, fuLen + 2, DEFAULT_PT, DEFAULT_MARK, 0);
			if (status < 0) {
				break;
			}
			start += fuLen;
		}
	}

	if (status < 0) {
		std::cout << jrtplib::RTPGetErrorString(status) << std::endl;
	}

	return status;
}

int VideoSender::addDest(const char* addr, uint16_t port) {
	jrtplib::RTPIPv4Address dest(htonl(inet_addr(addr)), port);
	return session.AddDestination(dest);
}

int VideoSender::delDest(const char *addr, uint16_t port) {
	jrtplib::RTPIPv4Address dest(htonl(inet_addr(addr)), port);
	return session.DeleteDestination(dest);
}

VideoSender::~VideoSender() {
	session.Destroy();
}