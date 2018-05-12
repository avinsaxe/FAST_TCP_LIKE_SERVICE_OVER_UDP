#pragma once
#include "SenderSocket.h"
#include <WS2tcpip.h>
using namespace std;

SenderSocket::SenderSocket()
{
	start = timeGetTime();
	reinit();
}
void SenderSocket::reinit() {
	sock= socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	remote.sin_port = DEFAULT_PORT;
}
void SenderSocket::freeSocket() {
	closesocket(sock);
	remote.sin_port = DEFAULT_PORT;  //when we freesocket, we should change the remote sin port, else what happens is, we just return already connected, although it wasnt connected
}


//href:// taken from HW1 submission of my own code
string getIP(string hostName)
{
	struct sockaddr_in server;
	in_addr addr;
	char *hostAddr;
	int len = hostName.length();
	char* host = (char*)malloc((len + 1) * sizeof(char));
	strcpy(host, hostName.c_str());
	DWORD dwRetVal = inet_addr(host);
	struct hostent *host_ent = gethostbyname(host);
	if (dwRetVal == INADDR_NONE)
	{
		if (host_ent != NULL)
		{
			memcpy((char *)&(server.sin_addr), host_ent->h_addr, host_ent->h_length);
			addr.s_addr = *(u_long *)host_ent->h_addr;	//Taken from   https://msdn.microsoft.com/en-us/library/ms738524(VS.85).aspx 
			return inet_ntoa(addr);
		}
		else if (host_ent == NULL)
		{
			return "";
		}
	}
	return host;
}


int SenderSocket::open(char * destination_ip1, int magic_port, int sender_window_packets, LinkProperties * lp, int* dataSizeReceived)
{
	//Get the ip address from hostname
	string IP = getIP(destination_ip1);
	char* hostIP = (char*)malloc((IP.length() + 1) * sizeof(char));
	strcpy(hostIP, IP.c_str());
	hostIP[IP.length()] = 0;
	
	if(IP.length()==0 || IP==""){
		printf("[ %.3f ] --> target %s is invalid\n", (float(timeGetTime() - start) / 1000), destination_ip1);
		return INVALID_NAME;
	}
	
	if (sock != SOCKET_ERROR && remote.sin_port != DEFAULT_PORT) {
		printf("[ %.3f ] --> socket error %d", (float(timeGetTime() - start) / 1000),WSAGetLastError());
		return ALREADY_CONNECTED;
	}

	
	reinit();   //important because there are multiple calls to senderSocket

	
		
	//**Open UDP socket, Bind it to port 0, and then utilize sendto and receivefrom
	//referenced from class notes for hw2
	
	struct sockaddr_in local;
	memset(&local, 0, sizeof(local));

	//SETUP the local before using sendto and receivefrom
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = 0; 


	//are you sure? Or do we need to keep reducing iterations when few iterations have completed
	lp->bufferSize = sender_window_packets + MAX_ITERATIONS_SYN;

	if (bind(sock, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
		//handle errors
		cout << "Bind Failed " << WSAGetLastError()<<endl;
		freeSocket();
		return SOCKET_ERROR;
	}
	//href:// https://msdn.microsoft.com/en-us/library/windows/desktop/ms738524(v=vs.85).aspx


	memset(&remote, 0, sizeof(remote));
	inet_pton(AF_INET, hostIP, &remote.sin_addr);

	
	remote.sin_addr.s_addr = inet_addr(hostIP);
	remote.sin_port = htons(magic_port);   //important. Server s3.irl.tamu.edu running on Windows. No need to send htons()
	remote.sin_family = AF_INET;

	int connected=connect(sock, (struct sockaddr*)&remote, sizeof(struct sockaddr_in));
	if (connected == SOCKET_ERROR) {
		freeSocket();
		return SOCKET_ERROR;
	}


	//Syn Packet Construction
	SenderSynHeader senderSynHeader,*senderSynHeaderPtr;  //senderSynHeader will have space of its own, we will point ptr to it
	char* syn = (char*)malloc(sizeof(SenderSynHeader)*1);
	senderSynHeader.sdh.flags.SYN = 1; //Not required already set
	senderSynHeader.sdh.seq = 0; //for hw1, this is always be 0
	senderSynHeaderPtr = &senderSynHeader;

	memcpy(&(senderSynHeaderPtr->lp),lp,sizeof(LinkProperties));    //copied LinkProperties to SenderSynHeader
	//Now we need to copy the above SenderSynHeader to syn packet, which we are creating
	memcpy(syn,senderSynHeaderPtr,sizeof(SenderSynHeader));

	
	int iteration = 0;
	float rto_timeout = 1.0f;
	rto = rto_timeout*1000000;
	//string ip(destination_ip);
	while (++iteration <= MAX_ITERATIONS_SYN) {
		
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = rto;
		
		fd_set fd;
		FD_ZERO(&fd);  //set the fd
		FD_SET(sock, &fd); //set the sock

		DWORD start_sendto = timeGetTime();
		printf("[ %.3f ] --> SYN 0 (attempt %d of %d, RTO %.3f) to %s\n",(float(start_sendto-start)/1000),iteration,MAX_ITERATIONS_SYN, rto_timeout, hostIP);
		int sendtostatus=sendto(sock, syn, sizeof(SenderSynHeader), 0, (struct sockaddr*)&remote,sizeof(remote));
		
		if (sendtostatus == SOCKET_ERROR) {
			freeSocket();
			free(syn);
			printf("[ %.3f ] --> failed sendto with %d\n", (float(timeGetTime() - start) / 1000),WSAGetLastError());
			return FAILED_SEND;
		}
		int totalSizeOnSelect=select(0, &fd, NULL, NULL, &timeout);
		if (totalSizeOnSelect < 0) {
			freeSocket();
			free(syn);
			printf("[ %.3f ] <-- failed select with %d\n", (float(timeGetTime() - start) / 1000), WSAGetLastError());
			return FAILED_RECV;
		}
		if (totalSizeOnSelect == 0) {
			continue;
		}
		//href:// taken from hw 2 of my own code
		//ack buffer creation
		char* ack = (char*)malloc(sizeof(ReceiverHeader));
		struct sockaddr_in acknowledgement;
		int len = sizeof(acknowledgement);
		int received=recvfrom(sock,ack,sizeof(ReceiverHeader),0,(struct sockaddr *)&acknowledgement,&len);
		DWORD end_receiveFrom = timeGetTime();
		if (received == SOCKET_ERROR) {
			printf("[ %.3f ] <-- failed recvfrom with %d\n", (float(end_receiveFrom - start) / 1000), WSAGetLastError());
			freeSocket();
			free(syn);
			free(ack);
			return FAILED_RECV;
		}

		ReceiverHeader* receiverHeader = (ReceiverHeader *)ack;
		int window_size = 0;
		if (receiverHeader->flags.ACK == 1 && receiverHeader->flags.SYN == 1) {
			window_size = receiverHeader->recvWnd;
		}

		//recompute RTO based on RTT
		double rtt_sample = double(end_receiveFrom - start_sendto)/1000; //time in seconds
		rto_timeout = (double)(3 * rtt_sample);	
		printf("[ %.3f ] <-- SYN-ACK 0 window 1; setting initial RTO to %.3f\n", (float(end_receiveFrom-start) / 1000), rto_timeout);
		*dataSizeReceived = received;
		rto = rto_timeout*1000000;
		free(syn);
		free(ack);
		return STATUS_OK;
	}
	return TIMEOUT;   //error handling here. All the errors will come down here, as the looping condition prevents from breaking on error
	
}

int SenderSocket::close(char * destination_ip1, int magic_port, int sender_window_packets, LinkProperties *lp)
{

	string IP = getIP(destination_ip1);
	char* hostIP = (char*)malloc((IP.length() + 1) * sizeof(char));
	strcpy(hostIP, IP.c_str());
	hostIP[IP.length()] = 0;

	if (IP.length() == 0 || IP == "") {
		printf("[ %.3f ] --> target %s is invalid\n", (float(timeGetTime() - start) / 1000), destination_ip1);
		return INVALID_NAME;
	}

	//Important condition for not being connected. No Socket Error and port number shouldnt be default port
	if (sock == SOCKET_ERROR) {
		printf("[ %.3f ] --> socket error %d", (float(timeGetTime() - start) / 1000), WSAGetLastError());
	}
	if (!(sock != SOCKET_ERROR && remote.sin_port != DEFAULT_PORT)){  //handled correctly. We have to print status from main
		//error in socket creation
		freeSocket();
		return NOT_CONNECTED;
	}
	
	//**Open UDP socket, Bind it to port 0, and then utilize sendto and receivefrom
	//referenced from class notes for hw2

	//href:// https://msdn.microsoft.com/en-us/library/windows/desktop/ms738524(v=vs.85).aspx

	lp->bufferSize = sender_window_packets +MAX_ITERATIONS_FIN;
	memset(&remote, 0, sizeof(remote));
	inet_pton(AF_INET, hostIP, &remote.sin_addr);
	remote.sin_addr.s_addr = inet_addr(hostIP);
	remote.sin_port = htons(magic_port);   //important. Server s3.irl.tamu.edu running on Windows. No need to send htons()
	remote.sin_family = AF_INET;
	
	//FIN Packet Construction
	SenderSynHeader senderFINHeader, *senderFINHeaderPtr;  //senderSynHeader will have space of its own, we will point ptr to it
	char* fin = (char*)malloc(sizeof(senderFINHeader) * 1);
	senderFINHeader.sdh.flags.FIN = 1; //Not required already set
	senderFINHeader.sdh.seq = 0; //for hw1, this is always be 0
	senderFINHeaderPtr = &senderFINHeader;
	memcpy(&(senderFINHeaderPtr->lp), lp, sizeof(LinkProperties));    //copied LinkProperties to SenderSynHeader 
	memcpy(fin, senderFINHeaderPtr, sizeof(senderFINHeader));	//Now we need to copy the above SenderSynHeader to syn packet, which we are creating

	int iteration = 0;
	float rto_timeout = ((float)rto/1000000);

	while (++iteration <= MAX_ITERATIONS_FIN) {
		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = (long)rto;		
		fd_set fd;
		FD_ZERO(&fd);  //set the fd
		FD_SET(sock, &fd); //set the sock
		DWORD start_sendto = timeGetTime();
		printf("[ %.3f ] --> FIN 0 (attempt %d of %d, RTO %.3f)\n", (float(start_sendto - start) / 1000), iteration,MAX_ITERATIONS_FIN, rto_timeout);
		int sendtostatus = sendto(sock, fin, sizeof(SenderSynHeader), 0, (struct sockaddr*)&remote, sizeof(remote));
		
		if (sendtostatus == SOCKET_ERROR) {
			//cout << "Socket Error " << WSAGetLastError() << endl;
			freeSocket();
			printf("[ %.3f ] --> failed sendto with %d\n", (float(start_sendto - start) / 1000), WSAGetLastError());
			return FAILED_SEND;
		}
		int totalSizeOnSelect = select(0, &fd, NULL, NULL, &timeout);
		if (totalSizeOnSelect < 0) {		
			freeSocket();
			printf("[ %.3f ] --> failed select with %d\n", (float(start_sendto - start) / 1000), WSAGetLastError());
			return FAILED_RECV;
		}
		if (totalSizeOnSelect == 0) {
			continue;
		}
		//href:// taken from hw 2 of my own code
		//ack buffer creation
		char* ack = (char*)malloc(sizeof(ReceiverHeader));
		struct sockaddr_in acknowledgement;
		int len = sizeof(acknowledgement);
		int received = recvfrom(sock, ack, sizeof(ReceiverHeader), 0, (struct sockaddr *)&acknowledgement, &len);
		DWORD end_receiveFrom = timeGetTime();
		if (received == SOCKET_ERROR) {
			//cout << "Failed on receive with " << WSAGetLastError() << endl;
			freeSocket();
			printf("[ %.3f ] <-- failed recvfrom with %d\n", (float(start_sendto - start) / 1000), WSAGetLastError());
			return FAILED_RECV;
		}
		ReceiverHeader* receiverHeader = (ReceiverHeader *)ack;
		int window_size = 0;
		if (receiverHeader->flags.ACK == 1 && receiverHeader->flags.FIN == 1) {
			window_size=receiverHeader->recvWnd;
		}
		printf("[ %.3f ] <-- FIN-ACK 0 window %d\n", (float(end_receiveFrom - start) / 1000),window_size);
		freeSocket();
		return STATUS_OK;
	}
	freeSocket();
	return TIMEOUT;   //error handling here. All the errors will come down here, as the looping condition prevents from breaking on error
}




int SenderSocket::send(char * buf, int bytes,int sequenceNum)
{
	if (sock == SOCKET_ERROR) {
		//error in socket creation
		cout << "Socket Error" << WSAGetLastError()<<endl;
		return SOCKET_ERROR;
	}
	SenderDataHeader senderDataHeader, *senderDataHeaderPtr; //senderSynHeader will have space of its own, we will point ptr to it
	char* data = (char*)malloc(MAX_PKT_SIZE*1);
	senderDataHeader.seq = sequenceNum; //for hw1, this is always be 0
	senderDataHeaderPtr = &senderDataHeader;
	memcpy((senderDataHeaderPtr), &senderDataHeader ,sizeof(SenderDataHeader));    //copied LinkProperties to SenderSynHeader 
	memcpy(data,senderDataHeaderPtr,sizeof(SenderDataHeader));  //adding data header to the packet
	memcpy(data+sizeof(SenderDataHeader),buf,bytes);	//Now we need to copy the above SenderSynHeader to syn packet, which we are creating
		
	fd_set fd;
	FD_ZERO(&fd);  //set the fd
	FD_SET(sock, &fd); //set the sock
	struct timeval timeout;
	timeout.tv_sec =0;
	timeout.tv_usec = (long)rto;

	int sendtostatus = sendto(sock, data, sizeof(SenderSynHeader)+bytes, 0, (struct sockaddr*)&remote, sizeof(remote));
	if (sendtostatus==SOCKET_ERROR) {
		cout << WSAGetLastError() << endl;
		freeSocket();
		return SOCKET_ERROR;
	}
	//cout << "Sendto Status " << sendtostatus << endl;

	return STATUS_OK;
}
