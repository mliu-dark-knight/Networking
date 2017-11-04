#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

typedef enum {False, True} bool;

extern int globalMyID;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
extern struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
extern int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
extern struct sockaddr_in globalNodeAddrs[256];

const int maxMsgLen = 2048;
extern const int INF;
extern int initCosts[256];
int minCosts[256] = {[0 ... 255] = 1 << 24};
int nextHops[256] = {[0 ... 255] = -1};
extern int Graph[256][256];
int seqNums[256] = {[0 ... 255] = -1};
int numNbs[256] = {[0 ... 255] = 0};
int globalSeqNum = 0;

extern FILE *flog;

pthread_mutex_t updateLock = PTHREAD_MUTEX_INITIALIZER;
bool needUpdate = False;

pthread_mutex_t floodLock = PTHREAD_MUTEX_INITIALIZER;
bool needFlood = False;

struct Pair {
	uint8_t destID;
	uint32_t cost;
} Pair;

struct LSP {
	uint8_t srcID;
	uint8_t numPairs;
	uint32_t seqNum;
	struct Pair pairs[256];
} LSP;


bool newLink(int id) {
//	gettimeofday(&globalLastHeartbeat[id], 0);
	bool link = False;
	if (Graph[globalMyID][id] != initCosts[id]) {
		if (Graph[globalMyID][id] == INF)
			link = True;
		Graph[globalMyID][id] = initCosts[id];
		pthread_mutex_lock(&updateLock);
		needUpdate = True;
		pthread_mutex_unlock(&updateLock);
		pthread_mutex_lock(&floodLock);
		needFlood = True;
		pthread_mutex_unlock(&floodLock);
	}
	return link;
}

bool areNbs(int ID1, int ID2) {
	if (ID1 < 0 || ID2 < 0)
		return False;
	if (Graph[ID1][ID2] == INF || ID1 == ID2)
		return False;
	return True;
}

bool isMyNb(int ID) {
	return areNbs(globalMyID, ID);
}


//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) { //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
			       (struct sockaddr *) &globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
		}
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 300 * 1000 * 1000; //300 ms
	while(1)
	{
		hackyBroadcast("HEREIAM", 7);
		nanosleep(&sleepFor, 0);
	}
}


void flood(char *message, int length, int fromID) {
//	printf("%d flood, lastHop %d\n", globalMyID, fromID);
	int i;
	for (i = 0; i < 256; i++) {
		if (isMyNb(i) && i != fromID && areNbs(fromID, i) == False) {
			sendto(globalSocketUDP, message, length, 0,
			       (struct sockaddr *) &globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));

		}
	}
}

int floodMsgLen(int numPairs) {
	return sizeof(struct Pair) * numPairs + 16;
}


//Returns number of byte to flood, 0 if old message
int handleFlood(char *message) {
	struct LSP lsp;
	memcpy(&lsp, message + 5, sizeof(LSP));
	int i;
	int srcID = lsp.srcID;
	int seqNum = lsp.seqNum;
	if (srcID == globalMyID || seqNum <= seqNums[srcID])
		return 0;
	seqNums[srcID] = seqNum;
	if (numNbs[srcID] != lsp.numPairs) {
		numNbs[srcID] = lsp.numPairs;
		pthread_mutex_lock(&updateLock);
		needUpdate = True;
		pthread_mutex_unlock(&updateLock);
	}
	for (i = 0; i < lsp.numPairs; i++) {
		int destID = lsp.pairs[i].destID;
		int cost = lsp.pairs[i].cost;
		if (cost != Graph[srcID][destID]) {
			if (needUpdate == False) {
				pthread_mutex_lock(&updateLock);
				needUpdate = True;
				pthread_mutex_unlock(&updateLock);
			}
			if ((Graph[srcID][destID] == INF && Graph[destID][srcID] == INF) && needFlood == False) {
				//none of them is in the connected component as myself
				if (nextHops[srcID] == -1 || nextHops[destID] == -1) {
					pthread_mutex_lock(&floodLock);
					needFlood = True;
					pthread_mutex_unlock(&floodLock);
				}
			}
		}
	}
	if (needUpdate == True) {
		for (i = 0; i < 256; i++)
			Graph[srcID][i] = INF;
		for (i = 0; i < lsp.numPairs; i++)
			Graph[srcID][lsp.pairs[i].destID] = lsp.pairs[i].cost;
	}
	return floodMsgLen(lsp.numPairs);
}

void printArr(int *array, int length) {
	int i;
	for (i = 0; i < length; i++)
		printf("%d ", array[i]);
	printf("\n");
}

void printGraph() {
	int i;
	printf("NodeID: %d\n", globalMyID);
	for (i = 0; i < 8; i++)
		printArr(Graph[i], 8);
}

void printDijkstra() {
	printf("NodeID: %d\n", globalMyID);
	printf("Cost\n");
	printArr(minCosts, 8);
	printf("Hop\n");
	printArr(nextHops, 8);
}

void initDijkstra() {
	int i;
	for (i = 0; i < 256; i++) {
		minCosts[i] = INF;
		nextHops[i] = -1;
	}
	minCosts[globalMyID] = 0;
	nextHops[globalMyID] = globalMyID;
}

void dijkstra() {
	if (needUpdate == False)
		return;
	pthread_mutex_lock(&updateLock);
	needUpdate = False;
	pthread_mutex_unlock(&updateLock);
//	struct timeval start;
//	gettimeofday(&start, 0);
	int lastHops[256] = {[0 ... 255] = INF};
	bool processed[256] = {[0 ... 255] = False};
	initDijkstra();
	while (True) {
		if (needUpdate == True)
			return;
		int min = INF;
		int u = -1;
		int i;
		for (i = 0; i < 256; i++) {
			if (processed[i] == False && minCosts[i] < min) {
				min = minCosts[i];
				u = i;
			}
		}
		if (min == INF)
			break;
		processed[u] = True;
		for (i = 0; i < 256; i++) {
			if (Graph[u][i] != INF) {
				int alt = minCosts[u] + Graph[u][i];
				if (alt < minCosts[i] || (alt == minCosts[i] && u < lastHops[i])) {
					minCosts[i] = alt;
					lastHops[i] = u;
					if (isMyNb(i) == True && nextHops[i] == -1)
						nextHops[i] = i;
					else
						nextHops[i] = nextHops[u];
				}
			}
		}
	}
//	struct timeval end;
//	gettimeofday(&end, 0);
//	printf("dijkstra takes %ld us\n", (end.tv_usec - start.tv_usec));
//	printDijkstra();
}

void floodHelper(struct LSP *lsp) {
	int length = floodMsgLen(lsp->numPairs);
	char *message = (char*)malloc(length);
	memset(message, '\0', sizeof(message));
	memcpy(message, "flood", 5);
	memcpy(message + 5, lsp, sizeof(struct Pair) * lsp->numPairs + 8);
	flood(message, length, -1);
	free(message);
}


void floodNbs() {
//	struct timeval start;
//	gettimeofday(&start, 0);

	pthread_mutex_lock(&floodLock);
	needFlood = False;
	pthread_mutex_unlock(&floodLock);
	struct LSP lsp;
	lsp.srcID = globalMyID;
	int curPairs = 0;
	int i;
	for (i = 0; i < 256; i++) {
		//new update
		if (needFlood == True)
			return;
		if (isMyNb(i) == True) {
			lsp.pairs[curPairs].destID = i;
			lsp.pairs[curPairs].cost = Graph[globalMyID][i];
			curPairs ++;
		}
	}
	if (curPairs != 0) {
		lsp.numPairs = curPairs;
		lsp.seqNum = globalSeqNum;
		globalSeqNum ++;
		floodHelper(&lsp);
	}

//	struct timeval end;
//	gettimeofday(&end, 0);
//	printf("%d floodNbs takes %ld us\n", globalMyID, (end.tv_usec - start.tv_usec));
}



void* checkUpdate(void* unsedParam) {
	int dCounter = 0;
	int fCounter = 0;
	int dThreshold = 2; //200ms
	int fThreshold = 1; //100ms
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 100 * 1000 * 1000; //100 ms
	struct timeval curTime;

	while (1) {
		int i;
		for (i = 0; i< 256; i++) {
			gettimeofday(&curTime, 0);
			//400ms
			if (isMyNb(i) == True && curTime.tv_sec - globalLastHeartbeat[i].tv_sec > 1) {
				pthread_mutex_lock(&updateLock);
				needUpdate = True;
				pthread_mutex_unlock(&updateLock);
				pthread_mutex_lock(&floodLock);
				needFlood = True;
				pthread_mutex_unlock(&floodLock);
				Graph[globalMyID][i] = INF;
				Graph[i][globalMyID] = INF;
//				printf("link failure, srcID: %d, destID: %d\n", globalMyID, i);
			}
		}
		fCounter ++;
		dCounter ++;
		if (needFlood == True && fCounter == fThreshold) {
			floodNbs();
		}
		if (fCounter == fThreshold)
			fCounter = 0;
		//100ms
		if (needUpdate == True && dCounter == dThreshold) {
			dijkstra();
		}
		if (dCounter == dThreshold)
			dCounter = 0;
		nanosleep(&sleepFor, 0);
	}
}

void handleSend(char *message, bool fwd) {
	int cmd_size = 4;
	int dest_size = 2;
	int destID = 0;
	char logLine[256];
	memset(logLine, '\0', 256);
	memcpy((char*)&destID + 2, message + cmd_size, dest_size);
	destID = ntohl(destID);

	//wait for dijkstraThread to finish
	if (needUpdate == True)
		dijkstra();

	int nextHop = nextHops[destID];
	if (nextHop == globalMyID) {
		if (fwd == False) {
			sprintf(logLine, "sending packet dest %d nexthop %d message %s\n",
			        destID, nextHop, message + cmd_size + dest_size);
			fwrite(logLine, 1, strlen(logLine), flog);
			fflush(flog);
		}
		sprintf(logLine, "receive packet message %s\n", message + cmd_size + dest_size);
		fwrite(logLine, 1, strlen(logLine), flog);
		fflush(flog);
	}
	else if (nextHop == -1) {
		sprintf(logLine, "unreachable dest %d\n", destID);
		fwrite(logLine, 1, strlen(logLine), flog);
		fflush(flog);
	}
	else {
		if (fwd == False) {
			sprintf(logLine, "sending packet dest %d nexthop %d message %s\n",
					destID, nextHop, message + cmd_size + dest_size);
			memcpy(message, "fwd ", 4);
		}
		else
			sprintf(logLine, "forward packet dest %d nexthop %d message %s\n", destID,
					nextHop, message + cmd_size + dest_size);
		fwrite(logLine, 1, strlen(logLine), flog);
		fflush(flog);
		if (sendto(globalSocketUDP, message, maxMsgLen, 0,
				   (struct sockaddr *) &globalNodeAddrs[nextHop], sizeof(globalNodeAddrs[nextHop])) == -1) {
			sprintf(logLine, "unreachable dest %d\n", destID);
			fwrite(logLine, 1, strlen(logLine), flog);
			fflush(flog);
		}
	}
}


void handleCost(char *message) {
	int cmd_size = 4;
	int nb_size = 2;
	int cost_size = 4;
	int nbID = 0;
	int nbCost;
	memcpy((char*)&nbID + 2, message + cmd_size, nb_size);
	memcpy(&nbCost, message + cmd_size + nb_size, cost_size);
	initCosts[ntohl(nbID)] = ntohl(nbCost);
}



void listenForNeighbors(FILE *flog) {
	char fromAddr[100];
	struct sockaddr_in theirAddr;
	socklen_t theirAddrLen;
	unsigned char recvBuf[maxMsgLen];
	int bytesRecvd;

	while (1) {
//		memset(recvBuf, '\0', maxMsgLen);
		theirAddrLen = sizeof(theirAddr);
		if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, maxMsgLen, 0,
								   (struct sockaddr *) &theirAddr, &theirAddrLen)) == -1) {
			perror("connectivity listener: recvfrom failed");
		}
		recvBuf[bytesRecvd] = '\0';

		inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);

		short int heardFrom = -1;
		if (strstr(fromAddr, "10.1.1.")) {
			heardFrom = atoi(
					strchr(strchr(strchr(fromAddr, '.') + 1, '.') + 1, '.') + 1);
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);

			//TODO: this node can consider heardFrom to be directly connected to it; do any such logic now.
			if (strncmp(recvBuf, "HEREIAM", 7) == 0) {
				if (newLink(heardFrom) == True) {
//					floodNbs();
//					if (lazyFlood == True)
//						floodNb(heardFrom);
				}
			} else if (strncmp(recvBuf, "fwd ", 4) == 0) {
//				printDijkstra();
				handleSend(recvBuf, True);
			} else if (strncmp(recvBuf, "flood", 5) == 0) {
				//TODO the same for HEREIAM
				newLink(heardFrom);
				//only reflood once
				int floodByte = handleFlood(recvBuf);
				if (floodByte != 0)
					flood(recvBuf, floodByte, heardFrom);

			}
			//record that we heard from heardFrom just now.
			gettimeofday(&globalLastHeartbeat[heardFrom], 0);
		}

		//Is it a packet from the manager? (see mp2 specification for more details)
		//send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
		else if (strncmp(recvBuf, "send", 4) == 0) {
			//TODO send the requested message to the requested destination node
			// ...
			handleSend(recvBuf, False);
		}
			//'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
		else if (strncmp(recvBuf, "cost", 4) == 0) {
			//TODO record the cost change (remember, the link might currently be down! in that case,
			//this is the new cost you should treat it as having once it comes back up.)
			// ...
			handleCost(recvBuf);
		}

		//TODO now check for the various types of packets you use in your own protocol
		//else if(!strncmp(recvBuf, "your other message types", ))
		// ...
	}
	//(should never reach here)
	close(globalSocketUDP);
}
