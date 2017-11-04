#include <fstream>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <thread>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>
#include <list>
#include <cmath>
#include <algorithm>
#include "utils.h"
#include <condition_variable>

using namespace std;
//priority_queue<uint64_t, vector<uint64_t>, greater<uint64_t>> loss_list;
list <pair<uint64_t, struct timespec>> loss_list;
mutex queue_mutex, testmtx;
condition_variable test;
unique_lock<mutex> lk(testmtx);

int socket_fd;

mutex thread_kill_mutex;
mutex first_ack_got_mutex;
bool first_ack_got = false;
bool thread_kill = false;
bool to_chk = false;

struct timespec recvdTime;
struct timespec currTime;
struct timespec sleepTime;

//struct timespec lastRecvdTime;
struct addrinfo *outservinfo = NULL;
uint64_t One_Way_Time = 20 * 1000 * 1000;
double alpha = 0.8;

void timeoutChker() {
	while (true) {
        {
            lock_guard<mutex> lg_tk(thread_kill_mutex);
            clock_gettime(CLOCK_MONOTONIC_RAW, &currTime);
			uint64_t threshold = 1250 * 1000 * 1000;
//			if (10 * One_Way_Time < 1000 * 1000 * 1000 * 1000) threshold = 10 * One_Way_Time;
//			else threshold = 3 * One_Way_Time;
			if (to_chk && (((currTime.tv_sec * 1000 * 1000 * 1000 + currTime.tv_nsec)
							- (recvdTime.tv_sec * 1000 * 1000 * 1000 + recvdTime.tv_nsec))
						   > threshold)) {
				thread_kill = true;
				test.notify_all();
				cerr << "Timeout!" << endl;
				return;
			} else if (thread_kill) {
				cerr << "Killed by main thread!" << endl;
				return;
			}
			sleepTime.tv_sec = 0;
			sleepTime.tv_nsec = (100 * 1000 * 1000);
        }
        nanosleep(&sleepTime, 0);
    }

}

void ackListener() {
    struct sockaddr_in theirAddr;
    socklen_t sock_struct_len = sizeof(theirAddr);
    char recvBuf[MAX_PKT_SIZE];
    memset(&theirAddr, 0, sizeof(theirAddr));
    ssize_t bytesRecvd;

    while(true) {

        {
            lock_guard<mutex> lg_tk(thread_kill_mutex);
            if(thread_kill) {
				test.notify_all();
                return;
            }
        }

        if (recvfrom(socket_fd, recvBuf, sizeof(recvBuf), MSG_PEEK,
                                   (struct sockaddr *) (&theirAddr), &sock_struct_len) > 0) {
			//cout << "[AckListener]: got something" << endl;
//            if (((struct sockaddr_in *) (outservinfo->ai_addr))->sin_family == theirAddr.sin_family &&
//                ((struct sockaddr_in *) (outservinfo->ai_addr))->sin_addr.s_addr ==
//                theirAddr.sin_addr.s_addr &&
//                ((struct sockaddr_in *) (outservinfo->ai_addr))->sin_port == theirAddr.sin_port &&
//                ((struct sockaddr_in *) (outservinfo->ai_addr))->sin_zero == theirAddr.sin_zero) {
                bytesRecvd = recvfrom(socket_fd, recvBuf, sizeof(recvBuf), 0,
                                       (struct sockaddr *) (&theirAddr), &sock_struct_len);
                recvBuf[bytesRecvd] = '\0';
			//cout << "[AckListener]: got something for us" << endl;
                if (!strncmp((const char *) recvBuf, "ackg", 4)) {
					//cout << "[AckListener]: got something for us and its an ack" << endl;
                    first_ack_got_mutex.lock();
                    first_ack_got = true;
                    first_ack_got_mutex.unlock();

					{
						lock_guard<mutex> lg_tk(thread_kill_mutex);
						to_chk = false;
//                        clock_gettime(CLOCK_MONOTONIC_RAW, &recvdTime);
					}


                    //memcpy(&lastRecvdTime, &recvdTime, sizeof(struct timespec));

                    struct header curr_header;
                    memcpy(&curr_header, recvBuf, sizeof(header));
//                    {
//                        lock_guard<mutex> lg_tk(thread_kill_mutex);
//						One_Way_Time = (uint64_t) floor(
//								(alpha) * ((recvdTime.tv_sec * 1000 * 1000 * 1000 + recvdTime.tv_nsec)
//										   - (curr_header.time.tv_sec * 1000 * 1000 * 1000 + curr_header.time.tv_nsec))
//								+ (1.0 - alpha) * One_Way_Time);
//						cout << "[AckListener]: 当前单向传输时间是: " << One_Way_Time / (1000 * 1000) << "ms" << endl;
//                    }

                    if(curr_header.length > 0) {
						//cout << "[AckListener]: list nonzero" << endl;
						{
							lock_guard<mutex> lg(queue_mutex);
							for (uint64_t i = 0; i < curr_header.length; ++i) {
								uint64_t curr_req_blk = *(uint64_t *) (recvBuf + sizeof(header) + i * sizeof(uint64_t));
								if (find_if(loss_list.begin(), loss_list.end(),
											[&](pair<uint64_t, struct timespec> obj) -> bool {
												return obj.first == curr_req_blk;
											}) == loss_list.end())
									loss_list.push_back(
											pair<uint64_t, struct timespec>(curr_req_blk, curr_header.time));
							}
						}
						if (!loss_list.empty()) test.notify_all();
                    } else {
						cout << "[AckListener]: list zero, ending!" << endl;

						{
							lock_guard<mutex> lg_tk(thread_kill_mutex);
							thread_kill = true;
						}

						test.notify_all();
                        return;
                    }
                }
			//}
        }

    }
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
	ifstream infile(filename, ifstream::binary);
	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 10 * One_Way_Time;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints)); //initalize the addrinfo struct
    hints.ai_family = AF_INET; //we want to connect on a TCP connect via IPv4
    hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

    //try to resolve the ip addr of the server that we want
    int retval;

	while (true) {
        retval = getaddrinfo(hostname, to_string(hostUDPport).c_str(), &hints, &outservinfo);
        if (retval == 0) {
            break;
        } else if(retval != EAI_AGAIN) {
            printf("ip addr resolution failed: %s\n", gai_strerror(retval));
            freeaddrinfo(outservinfo);
            return;
        }
    }

    struct addrinfo *p;
    for(p = outservinfo; p != NULL; p = p->ai_next) {
        //create a socket and connect to the server
        socket_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_fd == -1) {
			cout << "[Init] Error: Socket failed: " << strerror(errno) << endl;
            continue;
            //socket allocation failed, return
        }

        int yes = 1;
        int buffersize = MAX_PKT_SIZE * 100000;

		struct timeval recv_timeout;
		recv_timeout.tv_sec = 2;
		recv_timeout.tv_usec = 0;

        if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ||
            setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) == -1 ||
            setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(int)) == -1 ||
			setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &buffersize, sizeof(int)) == -1 ||
			setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(struct timeval)) == -1) {
            cout << "[Init] Error: Option Setting failed: " << strerror(errno) << endl;
            close(socket_fd);
            continue;
        }

        break;
    }

    if(p == NULL) {
        freeaddrinfo(outservinfo);
        return;
    }


	auto thread_ack = thread(ackListener);
	clock_gettime(CLOCK_MONOTONIC_RAW, &recvdTime);
	clock_gettime(CLOCK_MONOTONIC_RAW, &currTime);
	auto thread_to = thread(timeoutChker);


    if(!infile.is_open()) {
        cerr << "[FileOpen]: Error: " << strerror(errno) << endl;
    } else {

        infile.seekg (0, infile.end);
        auto file_length = infile.tellg();
		//cout << "Flen: " << file_length << endl;
        if(bytesToTransfer <= file_length) {
            file_length = bytesToTransfer;
        }

		if (file_length <= 0) {
			file_length = bytesToTransfer;
		}
        infile.seekg (0, infile.beg);

        while(true) {
			cout << "[Sending Header]" << endl;
            struct header request_pkt_hdr;
            memcpy(&(request_pkt_hdr.command), "reqt", strlen("reqt"));
            request_pkt_hdr.length = file_length;
            clock_gettime(CLOCK_MONOTONIC_RAW, &(request_pkt_hdr.time));

            sendToAll(&request_pkt_hdr, sizeof(request_pkt_hdr), p->ai_addr, socket_fd);

            first_ack_got_mutex.lock();
            if(first_ack_got) {
                first_ack_got_mutex.unlock();
                break;
            }

            first_ack_got_mutex.unlock();

			struct timespec temp;
			{
				lock_guard<mutex> lg_tk(thread_kill_mutex);
				memcpy(&temp, &sleepTime, sizeof(struct timespec));
				if (thread_kill) goto end;
			}
			nanosleep(&temp, 0);
        }

        while (true) {

            thread_kill_mutex.lock();
            if (thread_kill) {
                thread_kill_mutex.unlock();
                break;
            }

            thread_kill_mutex.unlock();
			//cout << loss_list.empty() << " " << thread_kill << endl;
			test.wait(lk, [] { return !loss_list.empty() || thread_kill || loss_list.empty(); });
			queue_mutex.lock();
            if (!loss_list.empty()) {

				char fileReadBuf[MAX_DATA_SIZE];
				uint64_t curr_blk_idx = loss_list.front().first;

//				cout << "[Sending Data]: " << curr_blk_idx << endl;
                infile.seekg(curr_blk_idx * MAX_DATA_SIZE, infile.beg);
                //seek to right spot

                auto block_size = MAX_DATA_SIZE;
				if (curr_blk_idx == (ceil((double) file_length / MAX_DATA_SIZE) - 1))
                    block_size = file_length - MAX_DATA_SIZE * (file_length / MAX_DATA_SIZE);
                auto curr_read_count = 0;
                while (curr_read_count < block_size && infile.good()) {
                    infile.read(fileReadBuf + curr_read_count, block_size - curr_read_count);
                    auto how_much_actually_read = infile.gcount();
                    curr_read_count += how_much_actually_read;
                }

				if (!infile.good() && curr_read_count < block_size) {
                    cerr << "[FileRead]: Error: " << strerror(errno) << endl;
                    break;
                }

                char buffer[MAX_PKT_SIZE];
                memcpy((buffer + sizeof(header)), fileReadBuf, block_size);
                struct header data_pkt_hdr;
                memcpy(&(data_pkt_hdr.command), "txpk", strlen("txpk"));
                data_pkt_hdr.length = curr_blk_idx;
				//clock_gettime(CLOCK_MONOTONIC_RAW, &(data_pkt_hdr.time));
				memcpy(&(data_pkt_hdr.time), &(loss_list.front().second), sizeof(struct timespec));
				memcpy(buffer, &data_pkt_hdr, sizeof(header));
				loss_list.pop_front();
                sendToAll(&buffer, block_size + sizeof(header), p->ai_addr, socket_fd);

			} else {
				{
					lock_guard<mutex> lg_tk(thread_kill_mutex);
					if (!to_chk) {
						clock_gettime(CLOCK_MONOTONIC_RAW, &recvdTime);
						to_chk = true;
					}
				}
			}
			queue_mutex.unlock();
        }
    }

	end:
	if (!thread_kill) {
		thread_kill_mutex.lock();
		thread_kill = true;

		thread_kill_mutex.unlock();
		test.notify_all();
	}

    thread_ack.join();
	thread_to.join();
    close(socket_fd);
    freeaddrinfo(outservinfo);
}

int main(int argc, char** argv)
{
	unsigned short int udpPort;
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		return 1;
	}
	udpPort = (unsigned short int)stoi(argv[2]);
	numBytes = stoll(argv[4]);
	
	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
} 
