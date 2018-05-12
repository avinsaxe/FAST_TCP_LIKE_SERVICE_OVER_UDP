/*
  Name: Avinash Saxena
  UIN: 426009625
*/


/* main.cpp
 * CSCE 463 Sample Code 
 * by Dmitri Loguinov
 */
#include "common.h"
#include "stdafx.h"
#include "SenderSocket.h"



//href:// Notes from classes, hw2 pdf


using namespace std;

 // this class is passed to all threads, acts as shared memory
class Parameters {
public:
	HANDLE	mutex;
	HANDLE	finished;
	HANDLE	eventQuit;
};




//href:// code design taken from first homework
void printInvalidArgs() {
	cout << "1. Destination server ip" << endl;
	cout << "2. Power of 2 buffer size" << endl;
	cout << "3. Sender window in packets" << endl;
	cout << "4. Round trip propogation delay(in sec)" << endl;
	cout << "5. Probability of loss in forward direction in sec" << endl;
	cout << "6. Probability of loss in backward direction in sec" << endl;
	cout << "7. Bottleneck link speed in Mbps" << endl;
}
string destination_ip = "";
int buffer_size_power = 0;
int sender_window_packets = 0;
double rtt = 0.0;
double prob_loss_fwd = 0.0;
double prob_loss_bwd = 0.0;
int bottleneck_speed = 0;

bool extractArguments(char *argv[]) {
	try {
		destination_ip = argv[1];
		buffer_size_power = stoi(argv[2]);
		sender_window_packets = stoi(argv[3]);
		rtt = stod(argv[4]);
		prob_loss_fwd = stod(argv[5]);
		prob_loss_bwd = stod(argv[6]);
		bottleneck_speed = stoi(argv[7]);
	}
	catch (exception e) {
		printInvalidArgs();
		return false;
	}
	return true;
}

bool checkValidity() {
	if (prob_loss_fwd >= 0.0 && prob_loss_fwd <= 1.0 && prob_loss_bwd >= 0.0 && prob_loss_bwd <= 1.0) {
		return true;
	}
	cout << "Probabilities should be in the range [0.0 1.0]" << endl;
	return false;
}
UINT64 minimum1(UINT64 a, UINT64 b) {
	if (a <= b) {
		return a;
	}
	return b;
}



int main(int argc, char *argv[]){
	if (argc != 8) {
		cout << "The number of arguments should be equal to 7, in the following order" << endl;
		printInvalidArgs();
		return 1;
	}
	bool extracted = extractArguments(argv);
	if (extracted == false) {
		return 1;
	}
	if (checkValidity() == false) {
		printInvalidArgs();
		return 1;
	}
	//href :// taken from previous HWs
	WSADATA wsaData = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "Main:\t WSAStartup Failed " <<WSAGetLastError()<<endl;
		return 1;
	}

	//copying the targethost
	char* targetHost = (char*)malloc(destination_ip.length() + 1);
	memcpy(targetHost, destination_ip.c_str(), destination_ip.length() + 1);
	targetHost[destination_ip.length()] = 0;

	string targetHostStr(targetHost);
	
	//href:// taken from hw3 pdf /*First 2 lines of output */

	
	printf("Main:\t sender W = %d, RTT %g sec, loss %g / %g, %d Mbps\n", sender_window_packets, rtt, prob_loss_fwd, prob_loss_bwd, bottleneck_speed);
	DWORD start = timeGetTime();
	UINT64 dwordBufSize = (UINT64)(1 << buffer_size_power);
	DWORD *dwordBuf = new DWORD[(int)dwordBufSize];
	printf("Main:\t initializing DWORD array with 2^%d elements... done in %d ms\n",buffer_size_power,timeGetTime()-start);
	
	

	//1st Call
	//href:// taken from hw3 pdf
	SenderSocket ss;
	int status = -1;
	LinkProperties lp;
	lp.RTT = rtt;
	lp.speed = 1000000*bottleneck_speed; // convert to megabits
	lp.pLoss[FORWARD_PATH] = prob_loss_fwd;
	lp.pLoss[RETURN_PATH] = prob_loss_bwd;
	int R = 3; //maximum number of retransmissions
	lp.bufferSize = sender_window_packets + R;
	int length_received = 0;
	DWORD start_open_main = timeGetTime();
	if ((status = ss.open(targetHost, MAGIC_PORT, sender_window_packets, &lp,&length_received)) != STATUS_OK) {
		cout << "Main:\t connect failed  with status " << status << endl;
		return status;
	}
	DWORD time_end_open = timeGetTime();
	//href:// taken from hw3 pdf
	char *charBuf = (char*)dwordBuf; // this buffer goes into socket
	UINT64 byteBufferSize = dwordBufSize << 2; // convert to bytes
	UINT64 off = 0; // current position in buffer

	printf("Main: \t connected to %s in %.3f sec,", argv[1], (float)(timeGetTime() - start_open_main) / 1000);
	cout << " pkt size "<<MAX_PKT_SIZE <<" bytes " << endl;
	int sequenceNum = 0;
	DWORD start_close_time = timeGetTime();
	if ((status = ss.close(targetHost,MAGIC_PORT, sender_window_packets,&lp)) != STATUS_OK)
	{
		printf("Main: \t close failed with status %d \n", status);
		return status;
	}
	printf("Main: \t transfer finished in %.3f sec\n",(float)(start_close_time - time_end_open) / 1000);
	
	
	//compulsorily call WSACleanup() at the end of main
	WSACleanup();
	return STATUS_OK;
}


/*

Questions:
1. What happens in case of multiple open requests. Do I quit, or just mention that already connected?
2. I only need to connect once right?
3. Do we have to do bind necessarily?
4. What about select?
5. How to recreate all the errors?
*/

/*
127.0.0.1
ts


*/