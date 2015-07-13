/* 
 * File:   rudp_events.h
 * Author: bkotte
 * Defines structures and methods for all RUDP events like send,acknowledge,timeout and resend
 * Created on May 23, 2015, 2:21 PM
 */

#ifndef RUDP_EVENTS_H
#define	RUDP_EVENTS_H
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/socket.h>
#include<sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <list>
#include <vector>
#include "event.h"
#include "rudp.h"
#include "rudp_api.h"
#include "getaddr.h"
#include "sockaddr6.h"
#include "vsftp.h"
using namespace std;

struct RudpSocket;

struct RudpResendPacket {
    char *data;
    int dataLength;
    u_int8_t retransmissions;
    RudpSocket *rudpSocket;
    struct sockaddr_in6 *destinationAddress;

    RudpResendPacket() {
        data = 0;
        dataLength = 0;
        retransmissions = 0;
        rudpSocket = 0;
        destinationAddress = 0;
    }

    RudpResendPacket(RudpSocket *rudpSocket, struct sockaddr_in6 *destinationAddress, char * data, int dataLength) {
        this->data = data;
        this->dataLength = dataLength;
        this->retransmissions = 0;
        this->rudpSocket = rudpSocket;
        this->destinationAddress = destinationAddress;
    }
};

struct TimeoutParams {
    u_int32_t seqno;
    RudpResendPacket * rudpPacketAddress;

    TimeoutParams() {
        seqno = -1;
        rudpPacketAddress = 0;
    }

    TimeoutParams(u_int32_t seqno, RudpResendPacket * rudpPacketAddress) {
        this->seqno = seqno;
        this->rudpPacketAddress = rudpPacketAddress;
    }
};

struct RudpPeer {
    struct sockaddr_in6 *address;
    u_int32_t windowStart;
    u_int32_t windowEnd;
    int lastDataSentIndex;
    u_int32_t lastAckSentByRecvr;
    u_int8_t isFtpBegin;
    u_int8_t isSyn;
    u_int8_t isFinSent;
    u_int8_t isEnded;
    u_int8_t isEndSent;
    u_int32_t initSeq;
    list<TimeoutParams*> timeoutList;

    RudpPeer() {
        windowStart = 0;
        windowEnd = 0;
        isSyn = 0;
        isFinSent = 0;
        isEndSent = 0;
        lastDataSentIndex = -1;
    }

    RudpPeer(struct sockaddr_in6 *address) {
        this->address = address;
        windowStart = 0;
        windowEnd = 0;
        isSyn = 0;
        isFinSent = 0;
        isEndSent = 0;
        lastDataSentIndex = -1;
        initSeq = rand() % 20000;
        cout << "Initial sequence number selected for this peer: " << initSeq << endl;
    }

    void pushTimeoutParams(u_int32_t seqno, RudpResendPacket *packet) {
        TimeoutParams *tp = new TimeoutParams(seqno, packet);
        timeoutList.push_front(tp);
    }
};

struct RudpData {
    struct vsftp *data;
    int dataLength, index;

    RudpData(struct vsftp *data, int len, int index) {
        this->data = (struct vsftp *) malloc(len);
        memcpy(this->data, data, len);
        this->dataLength = len;
        this->index = index;
    }

    RudpData() {
    }
};

struct RudpSocket {
    int udpFd;
    u_int8_t isDataAvailable;
    u_int8_t isClosed;
    int lastDataIndex;
    map<struct sockaddr_in6*, RudpPeer*> peerList;
    map<struct sockaddr_in6*, RudpPeer*>::iterator it;
    vector<RudpData*> dataList;
    struct sockaddr_in6* firstPeer;

    int (*receivePacketHandler)(rudp_socket_t, struct sockaddr_in6 *, void *, int);
    int (*eventHandler)(rudp_socket_t, rudp_event_t, struct sockaddr_in6 *);

    RudpSocket() {
        udpFd = -1;
        isDataAvailable = 0;
        lastDataIndex = 0;
        isClosed = 0;
    }

    void printMap() {
        cout << "****Printing Peerlist****\n";
        for (it = peerList.begin(); it != peerList.end(); ++it)
            std::cout << ntohs(it->first->sin6_port) << " => " << ntohs(it->second->address->sin6_port) << '\n';
    }
};

int isDataTransmitted(RudpSocket *rudpSocket);
int closeSocket(RudpSocket *rudpSocket, int isTimedout = 0);
int packetResend(int sockfd, void* arg);
int setCallbackForTimeout(struct RudpResendPacket * rudpResendPacket, char* eventName);
int removeCallbackForTimeout(RudpPeer * rudpPeer, u_int32_t seqno);
int packetSend(RudpSocket *rudpSocket, RudpPeer *activepeer, char * rudpPacket, int packetLength, u_int8_t retransmitFlag, int isFirst = 0);
char * createRudpDataPacket(struct vsftp *data, int dataLength, int seqno, int packetLength);
int transmitWindowData(RudpSocket *rudpSocket, RudpPeer *activePeer);
char* createRudpHeader(u_int32_t seqno, u_int16_t type);
int procACKPkt(RudpSocket * rudpSocket, rudp_hdr * rudpHeader, struct sockaddr_in6 *socketAddress);
int procSYNPkt(struct sockaddr_in6 *socketAddress, rudp_hdr * rudpHeader, RudpSocket * rudpSocket);
int procDataPkt(struct sockaddr_in6 *socketAddress, rudp_hdr * rudpHeader, RudpSocket * rudpSocket, void * data, int dataLength);
int procFINPkt(struct sockaddr_in6 *socketAddress, rudp_hdr * rudpHeader, RudpSocket * rudpSocket);
int processReceivedPacket(int udpFd, void *args);
int send(int fd, const void *buf, size_t n, int flags, __CONST_SOCKADDR_ARG addr, socklen_t addr_len);

#endif	/* RUDP_EVENTS_H */
