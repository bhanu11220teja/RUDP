#include "rudp_events.h"

static time_t randt;
static int counter = 0;

int isDataTransmitted(RudpSocket *rudpSocket) {
    //cout << "Inside isDataTransmitted()\n";
    for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
        if (rudpSocket->it->second->isEnded == 0) {
            //cout << "Data transmission incomplete\n";
            return 0;
        }
    }
    return 1;
}

int closeSocket(RudpSocket *rudpSocket, int isTimedout = 0) {
    //cout << "Inside closeSocket()\n";
    if (rudpSocket == NULL || rudpSocket->isClosed == 1) {
        return -1;
    }
    if (isTimedout == 1 && rudpSocket->eventHandler != NULL) {
        rudpSocket->eventHandler(rudpSocket, RUDP_EVENT_TIMEOUT, NULL);
        return 0;
    }
    //cout << "Trying to close socket\n";
    event_fd_delete(&processReceivedPacket, (void*) rudpSocket);
    rudpSocket->isClosed = 1;
    if (rudpSocket->eventHandler != NULL) {
        rudpSocket->eventHandler(rudpSocket, RUDP_EVENT_CLOSED, NULL);
    }
    return 0;
}

int packetResend(int sockfd, void* arg) {
    //cout << "Inside packetResend()\n";
    //cout << "Packet address for retransmission: " << arg << endl;
    for (int i = 0; i < RUDP_MAXRETRANS; i++) {
        event_timeout_delete(&packetResend, arg);
    }
    //cout << "Timeout event for the packet deleted\n";
    RudpResendPacket *rudpResendPacket = (RudpResendPacket *) arg;
    (rudpResendPacket->retransmissions)++;
    //cout << "No. of retransmissions incremented by 1. Current value: " << rudpResendPacket->retransmissions << endl;



    if (rudpResendPacket->retransmissions <= RUDP_MAXRETRANS) {
        struct rudp_hdr * rudpHeader = (struct rudp_hdr *) rudpResendPacket->data;

        /*cout << "Received Resend packet with sequence no: " << rudpHeader->seqno
                << "; Version: " << rudpHeader->version
                << ";Type: " << rudpHeader->type << endl;*/
        int seq = rudpHeader->seqno;
        char *newData;

        if (rudpHeader->type == RUDP_DATA) {
            newData = new char[rudpResendPacket->dataLength];
            memcpy(newData, rudpResendPacket->data, rudpResendPacket->dataLength);
            char *newHeader = createRudpHeader(rudpHeader->seqno, rudpHeader->type);
            memcpy(newData, newHeader, sizeof (struct rudp_hdr));
            rudpHeader = (struct rudp_hdr *) newHeader;

            /*cout << "Modified Resend packet with sequence no: " << rudpHeader->seqno
                    << "; Version: " << rudpHeader->version
                    << ";Type: " << rudpHeader->type << endl;*/

        } else {
            newData = createRudpHeader(rudpHeader->seqno, rudpHeader->type);
        }
        if (send(rudpResendPacket->rudpSocket->udpFd, newData, rudpResendPacket->dataLength, 0,
                (struct sockaddr *) rudpResendPacket->destinationAddress,
                sizeof (*rudpResendPacket->destinationAddress)) == -1) {
            //cout << "Retransmission failed";
            return -1;
        }
        char *eventName = new char[50];
        sprintf(eventName, "Timeout for Sequence No: %d", seq);
        setCallbackForTimeout(rudpResendPacket, eventName);
    } else {
        //cout << "Maximum no. of retransmissions failed\n";
        closeSocket(rudpResendPacket->rudpSocket, 1);
        return -1;
    }
    return 0;
}

int setCallbackForTimeout(RudpResendPacket * rudpResendPacket, char* eventName) {
    //cout << "Inside setCallbackForTimeout()\n";
    //cout << "Pointer of function: " << &packetResend << "\t argument: " << rudpResendPacket << endl;
    if (event_timeout(RUDP_TIMEOUT, &packetResend, (void*) rudpResendPacket, eventName) == -1) {
        //cout << "Timeout cannot be set\n";
        return -1;
    }
    return 0;
}

int removeCallbackForTimeout(RudpPeer * rudpPeer, u_int32_t seqno) {
    //cout << "Inside removeCallbackForTimeout()\n";
    if (rudpPeer->timeoutList.size() == 0) {
        //cout << "No active timers\n";
        return 0;
    }
    //cout << "Removing timer for packet with sequence no less than " << seqno << endl;
    for (list<TimeoutParams*>::iterator iterator = rudpPeer->timeoutList.begin(); iterator != rudpPeer->timeoutList.end(); ++iterator) {
        if ((*iterator)->seqno < seqno) {
            //cout << "Removing timer for packet with sequence no: " << (*iterator)->seqno << "\t for peer with address: " << (*iterator)->rudpPacketAddress << endl;
            for (int i = 0; i < RUDP_MAXRETRANS; i++) {
                event_timeout_delete(&packetResend, (*iterator)->rudpPacketAddress);
            }
            //cout << "Removed a timer with sequence no: " << (*iterator)->seqno << endl;
            iterator = rudpPeer->timeoutList.erase(iterator);
            iterator--;
        }
    }
    return 0;
}

int packetSend(RudpSocket *rudpSocket, RudpPeer * activePeer,
        char * rudpPacket, int packetLength,
        u_int8_t retransmitFlag, int isFirst = 0) {
    //cout << "Inside packetSend()\n";
    if (1 == isFirst) {
        srand((unsigned) time(&randt));
    }
    struct rudp_hdr *rudpHeader;
    sockaddr_in6 tmp = *(activePeer->address);
    /*//cout << "Normal send sequence no: " << rudpHeader->seqno
            << "; Version: " << rudpHeader->version
            << ";Type: " << rudpHeader->type << endl;*/

    if (send(rudpSocket->udpFd, rudpPacket, packetLength, 0, (struct sockaddr *) activePeer->address, sizeof (tmp)) == -1) {
        //cout << "Unable to send packet\n";
        return -1;
    }
    if (retransmitFlag != 0) {
        RudpResendPacket *rudpResendPacket = new RudpResendPacket(rudpSocket, activePeer->address, rudpPacket, packetLength);
        rudpHeader = (struct rudp_hdr *) rudpPacket;
        rudpHeader->version = ntohs(rudpHeader->version);
        rudpHeader->type = ntohs(rudpHeader->type);
        rudpHeader->seqno = ntohl(rudpHeader->seqno);
        activePeer->pushTimeoutParams(rudpHeader->seqno, rudpResendPacket);
        //cout << "Resend packet with sequence no: " << rudpHeader->seqno
        //        << "; Version: " << rudpHeader->version
        //        << ";Type: " << rudpHeader->type << endl;
        char *eventName = new char[50];
        sprintf(eventName, "Timeout for sequence no: %d", rudpHeader->seqno);
        if (setCallbackForTimeout(rudpResendPacket, eventName) == -1) {
            //cout << "Timeout cannot be set\n";
            return -1;
        }
    }
    return 0;
}

char * createRudpDataPacket(struct vsftp *data, int dataLength, int seqno, int *packetLength) {
    //cout << "Inside createRudpDataPacket()\n";
    char* ftpData = (char*) data;
    *packetLength = (int) sizeof (struct rudp_hdr) +dataLength;
    ftpData = malloc(*packetLength);
    char* rudpHeader = createRudpHeader(seqno, RUDP_DATA);
    memcpy(ftpData, rudpHeader, sizeof (struct rudp_hdr));
    data->vs_type = htonl(data->vs_type);
    memcpy(ftpData + sizeof (struct rudp_hdr), (char *) data, dataLength);
    data->vs_type = ntohl(data->vs_type);
    return ftpData;
}

int transmitWindowData(RudpSocket *rudpSocket, RudpPeer *activePeer) {
    //cout << "Inside transmitWindowData()\n";
    RudpData * tmp;
    char *rudpPacket;
    int packetLength;
    u_int32_t seqno;
    activePeer->windowStart = 1;
    activePeer->windowEnd = MIN(RUDP_WINDOW, rudpSocket->dataList.size());
    for (int i = activePeer->windowStart; i <= activePeer->windowEnd; i++) {
        tmp = rudpSocket->dataList.at(i);
        seqno = tmp->index + activePeer->initSeq + 1;
        rudpPacket = createRudpDataPacket(tmp->data, tmp->dataLength, seqno, &packetLength);
        if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 1) == -1) {
            return -1;
        }
        //cout << "Successfully sent packet with sequence no: " << seqno << endl;
        activePeer->lastDataSentIndex++;
    }
    return 0;
}

char* createRudpHeader(u_int32_t seqno, u_int16_t type) {
    //cout << "Inside createRudpHeader() with seqo: " << seqno << endl;
    struct rudp_hdr *rudpHeader = (struct rudp_hdr *) malloc(sizeof (struct rudp_hdr *));
    rudpHeader->version = htons(RUDP_VERSION);
    rudpHeader->seqno = htonl(seqno);
    rudpHeader->type = htons(type);
    return (char*) rudpHeader;
}

int procACKPkt(RudpSocket * rudpSocket, rudp_hdr * rudpHeader, struct sockaddr_in6 *socketAddress) {
    //cout << "Inside procACKPkt()\n";
    int packetLength;
    RudpData *dataNode, *tmp;
    RudpPeer *activePeer;
    char *rudpPacket;
    u_int32_t newStartIndex, newEndIndex, seqno;
    for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
        if (sockaddr6_cmp(socketAddress, rudpSocket->it->first) == 0) {
            activePeer = rudpSocket->it->second;
            break;
        }
    }
    if (rudpSocket->it == rudpSocket->peerList.end()) {
        //cout << "ACK packet from unknown peer: \n " << ntohs(socketAddress->sin6_port) << endl;
        return -1;
    }

    removeCallbackForTimeout(activePeer, rudpHeader->seqno);

    //Process SYN-ACK
    if (activePeer->isSyn == 0 && rudpHeader->seqno == activePeer->initSeq + 1) {
        //cout << "ACK received for SYN; Sequence no: " << rudpHeader->seqno << endl;
        activePeer->isSyn = 1;
        dataNode = rudpSocket->dataList.front();
        rudpPacket = createRudpDataPacket(dataNode->data, dataNode->dataLength, activePeer->initSeq + 1, &packetLength);
        if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 1) == -1) {
            //cout << "Unable to send BEGIN packet\n";
            return -1;
        }
        activePeer->isFtpBegin = 1;
        return 0;
    }

    // Process BEGIN-ACK
    if (activePeer->isFtpBegin == 1 && rudpHeader->seqno == activePeer->initSeq + 2) {
        //cout << "ACK received for VS_TYPE_BEGIN; Sequence no: " << rudpHeader->seqno << endl;
        transmitWindowData(rudpSocket, activePeer);
        return 0;
    }

    // Process END-ACK
    if (activePeer->isFinSent == 0 && rudpSocket->isDataAvailable == 1
            && rudpHeader->seqno == activePeer->initSeq + rudpSocket->dataList.size() + 1) {
        //cout << "ACK received for VS_TYPE_END; Sequence no: " << rudpHeader->seqno << endl;
        activePeer->isEndSent = 1;
        //cout << "Sending FIN packet\n";
        rudpPacket = createRudpHeader(activePeer->initSeq + rudpSocket->dataList.size() + 1, RUDP_FIN);
        packetLength = sizeof (struct rudp_hdr);
        if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 1) == -1) {
            //cout << "Unable to send FIN packet\n";
            return -1;
        }
        activePeer->isFinSent = 1;
        return 0;
    }

    // Process FIN-ACK
    if (activePeer->isFinSent == 1 && rudpSocket->isDataAvailable == 1
            && (rudpHeader->seqno == activePeer->initSeq + rudpSocket->dataList.size() + 1 || rudpHeader->seqno == activePeer->initSeq + rudpSocket->dataList.size() + 2)) {
        //cout << "ACK received for FIN; Sequence no: " << rudpHeader->seqno << endl;
        activePeer->isEnded = 1;
        removeCallbackForTimeout(activePeer, rudpHeader->seqno + 2);
        if (isDataTransmitted(rudpSocket) == 1) {
            closeSocket(rudpSocket);
        }
        return 0;
    }

    // Process DATA-ACK
    //cout << "ACK received for DATA; Sequence no: " << rudpHeader->seqno << endl;
    if (activePeer->isEndSent = 0 && rudpHeader->seqno == activePeer->initSeq + rudpSocket->dataList.size()) {
        //cout << "Last data packet ACKed.. Sending VS_TYPE_END\n";
        dataNode = rudpSocket->dataList.back();
        rudpPacket = createRudpDataPacket(dataNode->data, dataNode->dataLength, activePeer->initSeq + rudpSocket->dataList.size() + 1, &packetLength);
        if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 1) == -1) {
            //cout << "Unable to send END packet\n";
            return -1;
        }
        return 0;
    }
    seqno = rudpHeader->seqno - activePeer->initSeq - 1;
    if (seqno >= activePeer->windowStart + 1 && seqno <= activePeer->windowEnd + 1) {
        newStartIndex = seqno;
        newEndIndex = MIN(newStartIndex + RUDP_WINDOW - 1, rudpSocket->dataList.size() - 1);
        //cout << "ACK received within window\n";
        int start = MAX(newStartIndex, activePeer->lastDataSentIndex + 1);
        for (int i = start; i <= newEndIndex; i++) {
            tmp = rudpSocket->dataList.at(i);
            seqno = tmp->index + activePeer->initSeq + 1;
            rudpPacket = createRudpDataPacket(tmp->data, tmp->dataLength, seqno, &packetLength);
            if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 1) == -1) {
                //cout << "Unable to send data packet\n";
                return -1;
            }
            //cout << "Successfully sent DATA with Sequence no: " << seqno << endl;
            activePeer->lastDataSentIndex++;
        }
        activePeer->windowStart = newStartIndex;
        activePeer->windowEnd = newEndIndex;
    }
    return 0;
}

int procDataPkt(struct sockaddr_in6 *socketAddress, rudp_hdr * rudpHeader, RudpSocket * rudpSocket, void * data, int dataLength) {
    //cout << "Inside procDataPkt()\n";
    char *rudpPacket;
    int packetLength;
    RudpPeer * activePeer;
    for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
        if (sockaddr6_cmp(socketAddress, rudpSocket->it->first) == 0) {
            activePeer = rudpSocket->it->second;
            break;
        }
    }
    if (rudpSocket->it == rudpSocket->peerList.end()) {
        //cout << "Data packet received from unknown peer: " << ntohs(socketAddress->sin6_port) << endl;
        return -1;
    }
    //cout << "Data packet Sequence number: " << rudpHeader->seqno << endl;
    //cout << "Last ACK sent by receiver: " << activePeer->lastAckSentByRecvr << endl;

    if (rudpHeader->seqno == activePeer->lastAckSentByRecvr) {
        rudpPacket = createRudpHeader(rudpHeader->seqno + 1, RUDP_ACK);
        packetLength = sizeof (struct rudp_hdr);
        if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 0) == -1) {
            //cout << "Unable to send ACK packet\n";
            return -1;
        }

        activePeer->lastAckSentByRecvr++;
        if (rudpSocket->receivePacketHandler != NULL) {
            rudpSocket->receivePacketHandler(rudpSocket, socketAddress, data, dataLength);
        }
    } else {
        //cout << "Data packet not in order\n";
        rudpPacket = createRudpHeader(activePeer->lastAckSentByRecvr, RUDP_ACK);
        packetLength = sizeof (struct rudp_hdr);
        if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 0) == -1) {
            //cout << "Unable to send ACK packet\n";
            return -1;
        }
    }
    return 0;
}

int procSYNPkt(struct sockaddr_in6 *socketAddress, rudp_hdr * rudpHeader, RudpSocket * rudpSocket) {
    //cout << "Inside procSYNPkt()\n";
    srand((unsigned) time(&randt));
    char *rudpPacket;
    int packetLength;
    RudpPeer * activePeer;
    for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
        if (sockaddr6_cmp(socketAddress, rudpSocket->it->first) == 0) {
            activePeer = rudpSocket->it->second;
            break;
        }
    }
    if (rudpSocket->it == rudpSocket->peerList.end()) {
        //cout << "SYN packet received from peer: " << ntohs(socketAddress->sin6_port) << endl;
        activePeer = new RudpPeer(socketAddress);
        rudpSocket->peerList[socketAddress] = activePeer;
        activePeer->lastAckSentByRecvr = rudpHeader->seqno + 1;
    }
    activePeer->isSyn = 1;
    rudpPacket = createRudpHeader(rudpHeader->seqno + 1, RUDP_ACK);
    packetLength = sizeof (struct rudp_hdr);
    if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 0) == -1) {
        //cout << "Unable to send SYN-ACK packet\n";
        return -1;
    }
    return 0;
}

int procFINPkt(struct sockaddr_in6 *socketAddress, rudp_hdr * rudpHeader, RudpSocket * rudpSocket) {
    //cout << "Inside procFINPkt()\n";
    char *rudpPacket;
    int packetLength;
    RudpPeer * activePeer;
    for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
        if (sockaddr6_cmp(socketAddress, rudpSocket->it->first) == 0) {
            activePeer = rudpSocket->it->second;
            break;
        }
    }
    if (rudpSocket->it == rudpSocket->peerList.end()) {
        //cout << "FIN packet received from unknown peer: " << ntohs(socketAddress->sin6_port) << endl;
        return -1;
    }
    //cout << "Recvd FIN Sequence no: " << rudpHeader->seqno << "; Last ACKed Sequence no: " << activePeer->lastAckSentByRecvr << endl;
    if (rudpHeader->seqno != activePeer->lastAckSentByRecvr) {
        //cout << "FIN packet with invalid Sequence number.. dropping\n";
        return -1;
    }
    rudpPacket = createRudpHeader(rudpHeader->seqno + 1, RUDP_ACK);

    //activePeer->lastAckSentByRecvr++;
    packetLength = sizeof (struct rudp_hdr);
    if (packetSend(rudpSocket, activePeer, rudpPacket, packetLength, 0) == -1) {
        //cout << "Unable to send FIN-ACK packet\n";
        return -1;
    }
    return 0;
}

int processReceivedPacket(int udpFd, void *args) {
    //cout << "Inside processReceivedPacket()\n";
    void* data;
    int dataLength;
    RudpSocket * rudpSocket = (RudpSocket *) args;
    struct sockaddr_in6* socketAddress = (struct sockaddr_in6 *) malloc(sizeof (struct sockaddr_in6));
    int socketAddressLength = sizeof (struct sockaddr_in6);
    char *headerData = new char[RUDP_MAXPKTSIZE + 1];
    int bytesRead = recvfrom(rudpSocket->udpFd, headerData, RUDP_MAXPKTSIZE, 0,
            (struct sockaddr *) socketAddress, (socklen_t *) & socketAddressLength);
    if (bytesRead < sizeof (rudp_hdr)) {
        //cout << "Packet with error received... dropping\n";
        return 0;
    }
    rudp_hdr * rudpHeader = (rudp_hdr *) headerData;
    //cout << "Sequence no: " << rudpHeader->seqno
    // << "; Version: " << rudpHeader->version
    // << ";Type: " << rudpHeader->type << endl;

    if (1 != rudpHeader->version && (rudpHeader->type < 1 || rudpHeader->type > 5)) {
        rudpHeader->seqno = ntohl(rudpHeader->seqno);
        rudpHeader->type = ntohs(rudpHeader->type);
        rudpHeader->version = ntohs(rudpHeader->version);
    }

    if (RUDP_VERSION != rudpHeader->version) {
        //cout << "Packet with invalid RUDP version received... dropping\n";
        //cout << "Sequence no: " << rudpHeader->seqno
        //        << "; Version: " << rudpHeader->version
        //        << ";Type: " << rudpHeader->type << endl;
        return 0;
    }

    if (rudpHeader->type == RUDP_SYN) {
        return procSYNPkt(socketAddress, rudpHeader, rudpSocket);
    } else if (rudpHeader->type == RUDP_ACK) {
        return procACKPkt(rudpSocket, rudpHeader, socketAddress);
    } else if (rudpHeader->type == RUDP_DATA) {
        dataLength = bytesRead - sizeof (rudp_hdr);
        if (0 == dataLength) {
            //cout << "Packet with error received... dropping\n";
            return 0;
        }
        data = (void *) (headerData + sizeof (rudp_hdr));
        return procDataPkt(socketAddress, rudpHeader, rudpSocket, data, dataLength);
    } else if (rudpHeader->type == RUDP_FIN) {
        return procFINPkt(socketAddress, rudpHeader, rudpSocket);
    } else {
        //cout << "Packet with invalid RUDP type received... dropping\n";
        return 0;
    }
}

int send(int fd, const void *buf, size_t n,
        int flags, __CONST_SOCKADDR_ARG addr,
        socklen_t addr_len) {
    //cout << "Inside send()\n";
    int returnValue = 1;
    returnValue = sendto(fd, buf, n, flags, addr, addr_len);
    return returnValue;
}