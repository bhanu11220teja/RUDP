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
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <map>
#include <string>

#include "event.h"
#include "rudp.h"
#include "rudp_api.h"
#include "getaddr.h"
#include "sockaddr6.h"
#include "rudp_events.cpp"

using namespace std;

/* 
 * rudp_socket: Create a RUDP socket. 
 * May use a random port by setting port to zero. 
 */

rudp_socket_t rudp_socket(int port) {
    int udpSocket;
    //cout << "Inside rudp_socket()\n";
    //cout << "****Creating new RUDP socket****\n";
    udpSocket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        //cout << "Error while socket creation\n";
        return NULL;
    }

    struct sockaddr_in6 udpSocketAddress;
    memset((char *) &udpSocketAddress, 0, sizeof (udpSocketAddress));
    udpSocketAddress.sin6_port = htons(port);
    udpSocketAddress.sin6_addr = in6addr_any;
    udpSocketAddress.sin6_family = AF_INET6;

    if (bind(udpSocket, (struct sockaddr *) &udpSocketAddress, sizeof (udpSocketAddress)) == -1) {
        //cout << "Error while binding socket\n";
        return NULL;
    }

    RudpSocket *rudpSocket = new RudpSocket();
    rudpSocket->udpFd = udpSocket;
    char* identifier = "ReceivePacket";
    if (event_fd(udpSocket, &processReceivedPacket, (void*) rudpSocket, identifier) < 0) {
        //cout << "Error while registering event for receiving packets\n";
        return NULL;
    }
    //cout << "Socket created successfully\n";
    return rudpSocket;
}

/* 
 *rudp_close: Close socket 
 */

int rudp_close(rudp_socket_t rsocket) {
    //cout << "Inside rudp_close()\n";
    RudpSocket *rudpSocket = (RudpSocket *) rsocket;
    if (rudpSocket->isDataAvailable == 1) {
        //cout << "Data packets to be sent: " << rudpSocket->dataList.size() << endl;
        for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
            if (rudpSocket->it->second->isFtpBegin == 0) {
                rudpSocket->it->second->isFtpBegin = 1;
                //cout << "SYN packet to be sent to peer\n";
                char *synPacket = createRudpHeader(rudpSocket->it->second->initSeq, RUDP_SYN);
                packetSend(rudpSocket, rudpSocket->it->second, synPacket, sizeof (struct rudp_hdr), 1, 1);
            }
        }
    } //else
        //cout << "Data not available\n";

    return 0;
}

/* 
 *rudp_recvfrom_handler: Register receive callback function 
 */
int rudp_recvfrom_handler(rudp_socket_t rsocket,
        int (*handler)(rudp_socket_t, struct sockaddr_in6 *,
        void *, int)) {
    //cout << "Inside rudp_recvfrom_handler()\n";
    RudpSocket *rudpSocket = (RudpSocket *) rsocket;
    rudpSocket->receivePacketHandler = handler;
    return 0;
}

/* 
 *rudp_event_handler: Register event handler callback function 
 */
int rudp_event_handler(rudp_socket_t rsocket,
        int (*handler)(rudp_socket_t, rudp_event_t,
        struct sockaddr_in6 *)) {
    //cout << "Inside rudp_event_handler()\n";
    RudpSocket *rudpSocket = (RudpSocket *) rsocket;
    rudpSocket->eventHandler = handler;
    return 0;
}

/* 
 * rudp_sendto: Send a block of data to the receiver. 
 */

int rudp_sendto(rudp_socket_t rsocket, void* data, int len, struct sockaddr_in6* to) {
    //cout << "Inside rudp_sendto()\n";
    RudpPeer * activePeer = NULL;
    RudpSocket *rudpSocket = (RudpSocket *) rsocket;
    struct vsftp * ftpData = (struct vsftp *) data;
    ftpData->vs_type = ntohl(ftpData->vs_type);
    for (rudpSocket->it = rudpSocket->peerList.begin(); rudpSocket->it != rudpSocket->peerList.end(); rudpSocket->it++) {
        if (sockaddr6_cmp(to, rudpSocket->it->first) == 0) {
            //cout << "Active peer found: " << ntohs(to->sin6_port) << endl;
            activePeer = rudpSocket->it->second;
            break;
        }
    }
    if (activePeer == NULL) {
        //cout << "No active peer found: " << ntohs(to->sin6_port) << endl;
        if (ftpData->vs_type != VS_TYPE_BEGIN) {
            //cout << "Active peer should have been present\n";
            return -1;

        }
        if (rudpSocket->peerList.size() == 0) {
            rudpSocket->firstPeer = to;
        }
        activePeer = new RudpPeer(to);
        rudpSocket->peerList[to] = activePeer;
        //cout << "New peer registered: " << ntohs(to->sin6_port) << endl;


    }
    if (rudpSocket->firstPeer == to) {
        //cout << "First peer so pushing to data list\n";
        RudpData *rudpData = new RudpData(ftpData, len, rudpSocket->dataList.size());
        rudpSocket->dataList.push_back(rudpData);
        if (ftpData->vs_type == VS_TYPE_END) {
            //cout << "VS_TYPE_END received\n";
            rudpSocket->isDataAvailable = 1;
        } else
            rudpSocket->lastDataIndex++;
    }
    ftpData->vs_type = htonl(ftpData->vs_type);
    return 0;
}

