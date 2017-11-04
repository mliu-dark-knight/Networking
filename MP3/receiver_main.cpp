#include <fstream>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <thread>
#include <iostream>
#include <math.h>
#include <mutex>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include "utils.h"

using namespace std;

int socket_fd;
extern const int MAX_DATA_SIZE;
struct sockaddr_in senderAddr;
vector<uint64_t > request_ids;
mutex request_lock;
uint64_t max_num_frame;
uint64_t next_frame;
//be careful of integer division
const int MAX_ID_SIZE = MAX_DATA_SIZE / sizeof(uint64_t);
const int max_split_req = 20;
const int min_split_req = 10;
int num_split_req = 20;
int window_size = num_split_req * MAX_ID_SIZE;
bool finished = false;
//request to send is received
bool reqt_recved = false;

const uint64_t ms = 1000 * 1000;
const uint64_t s = 1000 * ms;

timespec sleepFor;
uint64_t RTT = 20 * ms; //20ms
double alpha = 0.8;

const double reduce_ratio = 1.5;

void increaseWindow() {
	num_split_req *= reduce_ratio;
	if (num_split_req > max_split_req)
		num_split_req = max_split_req;
	window_size = num_split_req * MAX_ID_SIZE;
}

void reduceWindow() {
	num_split_req = ceil((double) num_split_req / reduce_ratio);
	if (num_split_req < min_split_req)
		num_split_req = min_split_req;
	window_size = num_split_req * MAX_ID_SIZE;
}

void updateRTT(Header &header) {
	timespec current;
	clock_gettime(CLOCK_MONOTONIC_RAW, &current);

	//do not divide by num_split_req
	uint64_t newRTT = (current.tv_sec * s + current.tv_nsec - header.time.tv_sec * s - header.time.tv_nsec);
//	if (newRTT > RTT)
	if (newRTT / ms > RTT / ms)
		reduceWindow();
	else
		increaseWindow();
//	cout << "RTT: " << RTT / ms << "ms " << "newRTT " << newRTT / ms << "ms" << endl;
//	cout << "current window size: " << window_size << endl;
	RTT = alpha * RTT + (1.0 - alpha) * newRTT;
//	cout << "[updateRTT]: current RTT: " << RTT / ms << "ms" << endl;
	if (RTT < 500 * ms) {
		sleepFor.tv_sec = 0;
		sleepFor.tv_nsec = 2 * RTT;
	}
	else if (RTT < s && RTT >= 500 * ms) {
		sleepFor.tv_sec = 1;
		sleepFor.tv_nsec = 0 * ms;
	}
	else if (RTT >= s + 0 * ms) {
		sleepFor.tv_sec = RTT / s;
		sleepFor.tv_nsec = RTT % s;
	}
//	else {
//		sleepFor.tv_sec = 0;
//		sleepFor.tv_nsec = 2 * RTT;
//	}
}


void sendAck() {
	request_lock.lock();

//	for(int item_in_q : request_ids)
//		cout << item_in_q << " ";
//	cout << endl;

	int limit = window_size / MAX_ID_SIZE;
	if (limit > ceil((long double) request_ids.size() / MAX_ID_SIZE))
		limit = ceil((long double) request_ids.size() / MAX_ID_SIZE);
	for (int i = 0; i < limit; i++) {
		Header sendHeader;
		char sendMsg[MAX_PKT_SIZE];
		uint64_t request_size = ((i + 1) * MAX_ID_SIZE < request_ids.size()) ? MAX_ID_SIZE : request_ids.size() - (i * MAX_ID_SIZE);

		memcpy(&sendHeader.command, "ackg", sizeof(sendHeader.command));
		sendHeader.length = request_size;
		clock_gettime(CLOCK_MONOTONIC_RAW, &sendHeader.time);
		memcpy(sendMsg, &sendHeader, sizeof(sendHeader));

//		cout << "[SendAck]: request size: " << request_size << endl;
		memcpy(sendMsg + sizeof(sendHeader), &request_ids[i * MAX_ID_SIZE], request_size * sizeof(uint64_t));

		int sendLen = sizeof(sendHeader) + request_size * sizeof(uint64_t);
		if (sendto(socket_fd, sendMsg, sendLen, 0, (struct sockaddr *) &senderAddr, sizeof(senderAddr)) == -1) {
			perror("connectivity receiver: sendTo failed");
			exit(1);
		}
	}
	request_lock.unlock();
}

void checkTimeout() {
	sleepFor.tv_sec = 1;
	sleepFor.tv_nsec = 0;

	nanosleep(&sleepFor, 0);

	while (true) {
//		cout << "next frame: " << next_frame << endl;
//		cout << "max frame: " << max_num_frame << endl;
		if (next_frame == max_num_frame && request_ids.size() == 0) {
			cout << "[ChkTimeout]: finished!" << endl;
			sendAck();
			finished = true;
			exit(0);
		}
		while (next_frame < max_num_frame && request_ids.size() < window_size) {
			request_lock.lock();
			request_ids.push_back(next_frame);
			request_lock.unlock();
			next_frame ++;
		}
		sendAck();
		nanosleep(&sleepFor, 0);
	}
}

//return true if start checking timeout
bool receiveFrame(char *message, int length, ofstream &outputfile) {
    Header recvHeader;
    memcpy((void*) &recvHeader, message, sizeof(recvHeader));

    if (strncmp(recvHeader.command, "reqt", sizeof(recvHeader.command)) == 0) {
//	    cout << "[receiveFrame]: reqt!" << endl;
	    if (reqt_recved)
		    return false;
	    reqt_recved = true;
	    max_num_frame = ceil((long double) recvHeader.length / MAX_DATA_SIZE);
//	    cout << "Max num frame: " << max_num_frame << endl;
		uint64_t loop_limit = max_num_frame;
		if (window_size < max_num_frame) loop_limit = window_size;
	    next_frame = loop_limit;

		for (uint64_t i = 0; i < loop_limit; i++)
		    request_ids.push_back(i);
	    sendAck();
	    return true;
    }

	if (strncmp(recvHeader.command, "txpk", sizeof(recvHeader.command)) == 0) {
		updateRTT(recvHeader);
//		cout << "[receiveFrame]: txpk!" << endl;
//		cout << "[receiveFrame]: block id " << recvHeader.length << endl;
		if (find(request_ids.begin(), request_ids.end(), recvHeader.length) == request_ids.end())
			return false;
		request_lock.lock();
		request_ids.erase(find(request_ids.begin(), request_ids.end(), recvHeader.length));
		request_lock.unlock();
		long pos = recvHeader.length * MAX_DATA_SIZE;
		outputfile.seekp(pos);
		outputfile.write(message + sizeof(recvHeader), length - sizeof(recvHeader));
		outputfile.flush();
	}
	return false;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
	ofstream outfile(destinationFile, ofstream::binary);

    struct addrinfo hints;
    struct addrinfo *outservinfo = NULL;

    memset(&hints, 0, sizeof(hints)); //initalize the addrinfo struct
    hints.ai_family = AF_INET; //we want to connect on a TCP connect via IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    //try to resolve the ip addr of the server that we want
    int retval;

    while (1) {
        retval = getaddrinfo(NULL, to_string(myUDPport).c_str(), &hints, &outservinfo);
        if (retval == 0) {
            break;
        } else if(retval != EAI_AGAIN) {
            printf("ip addr resolution failed: %s\n", gai_strerror(retval));
            goto client_cleanup_addr_lookup_fail;
        }
    }

    struct addrinfo *p;
    for (p = outservinfo; p != NULL; p = p->ai_next) {
        //create a socket and connect to the server
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd == -1) {
            continue;
            //socket allocation failed, return
        }

        int yes = 1;
        int buffersize = 1472 * 100000;
        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ||
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) == -1 ||
            setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(int)) == -1 ||
            setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &buffersize, sizeof(int)) == -1) {
            cout << "[Init] Error: Option Setting failed: " << strerror(errno) << endl;
            close(socket_fd);
            continue;
        }

        if (::bind(socket_fd, p->ai_addr, p->ai_addrlen) != 0) {
            close(socket_fd);
            continue;
            //connection failed, return
        }
        break;
    }

    if (p == NULL) {
        goto client_cleanup_addr_lookup_fail;
    }

	char recvMsg[MAX_PKT_SIZE];
    if (!outfile.is_open()) {
        cerr << "[FileOpen]: Error: " << strerror(errno) << endl;
        exit(1);
    }

    while (!finished) {
        memset(recvMsg, '\0', sizeof(recvMsg));
        socklen_t senderAddrLen = sizeof(senderAddr);
        int bytesRecvd = recvfrom(socket_fd, recvMsg, sizeof(recvMsg), 0, (struct sockaddr *) &senderAddr, &senderAddrLen);
        if (bytesRecvd == -1) {
            perror("connectivity receiver: recvfrom failed");
            exit(1);
        }
        recvMsg[bytesRecvd] = '\0';
	    if (receiveFrame(recvMsg, bytesRecvd, outfile)) {
		    thread timeout = thread(checkTimeout);
		    timeout.detach();
	    }
    }

    client_cleanup_addr_lookup_fail:
    freeaddrinfo(outservinfo);
}

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		return 1;
	}
	
	udpPort = (unsigned short int)stoi(argv[1]);
	
	reliablyReceive(udpPort, argv[2]);

}
