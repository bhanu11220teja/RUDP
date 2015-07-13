# RUDP

Done by:
Bhanu Teja Kotte    	    | 	btkotte@kth.se 				        
Debopam Bhattacherjee       |  	debopam@kth.se
   
RUDP or Reliable UDP (User Datagram Protocol) adds reliability on top of the unreliable best-effort UDP protocol which runs at the transport layer. 
Hence, RUDP wrapper adds the following additional features to UDP:

1. Flowcontrol using Sliding Window protocol
2. Detection of lost packets and ARQ-based retransmissions
3. Detection of out-of-order packets

A file transfer application using VSFTP (a Very Simple File Transfer Protocol) runs on top of the RUDP protocol in order to send and receive files to peers.

Programming languages used : C++ (g++ compiler)
Platform used : Ubuntu 14.04

File Description:

event.c,event.h used for asynchronous event handling

rudp_api.h Application generic API for RUDP

rudp.h defines the RUDP header structure

rudp_events.h,rudp_events.cpp the RUDP protocol implementaion

vsftp.h,vs_send.cpp,vs_recv.cpp The FTP application

Instructions:

Run the Makefile. It generates 2 object files vs_send and vs_recv

Usage:

./vs_recv [-d] port

./vs_send [-d] host1:port1 [host2:port2] ... file1 [file2]...

-d enables debugging

P.s: Receiver should be up and running before the sender starts
