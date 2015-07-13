CC = gcc
CXX = g++ -fpermissive
CFLAGS = -g -Wall -lstdc++
CXXFLAGS = -g

all: vs_send vs_recv
	
vs_send: vs_send.o rudp.o event.o getaddr.o sockaddr6.o
	$(CXX) $(CXXFLAGS) $^ -o $@

vs_recv: vs_recv.o rudp.o event.o getaddr.o sockaddr6.o
	$(CXX) $(CXXFLAGS) $^ -o $@
	
vs_send.o vs_recv.o rudp.o: rudp.cpp rudp.h rudp_api.h event.h getaddr.h sockaddr6.h vsftp.h
	$(CXX) $(CXXFLAGS) -c rudp.cpp vs_send.cpp vs_recv.cpp rudp_events.cpp rudp_events.h
	
getaddr.o: getaddr.c getaddr.h
	$(CC) $(CFLAGS) -c getaddr.c

sockaddr6.o: sockaddr6.c sockaddr6.h
	$(CC) $(CFLAGS) -c sockaddr6.c

event.o: event.c event.h
	$(CC) $(CFLAGS) -c event.c

#rudp.tar: vs_send.c vs_recv.c vsftp.h Makefile rudp_api.h rudp.h event.h \
	event.c rudp.c getaddr.c getaddr.h sockaddr6.c sockaddr6.h
#	tar cf rudp.tar

clean:
	/bin/rm -f vs_send vs_recv *.o *.gch rudp.tar
