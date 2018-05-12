#pragma once
#include "common.h"
#include "SenderDataHeader.h"

using namespace std;
//href:// taken from hw3 shared by prof. Dmitri

#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel

#define MAX_ITERATIONS_SYN 3
#define MAX_ITERATIONS_FIN 5

class SenderSocket {
public:
	long rto = 1000000l;
	struct sockaddr_in remote;
	int DEFAULT_PORT = 0;
	DWORD start;
	SOCKET sock;
	SenderSocket();
	void reinit();
	void freeSocket();
	int open(char * destination_ip, int magic_port, int sender_window_packets, LinkProperties * lp, int* dataSizeReceived);
	int close(char * destination_ip, int magic_port, int sender_window_packets, LinkProperties * lp);
	int send(char * buf, int bytes, int sequenceNum);
};
