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

void listenForNeighbors();
void* announceToNeighbors(void* unusedParam);
void* checkUpdate(void* unusedParam);

int globalMyID = 0;
//last time you heard from each node. TODO: you will want to monitor this
//in order to realize when a neighbor has gotten cut off from you.
struct timeval globalLastHeartbeat[256];

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

FILE *flog;

int initCosts[256] = {[0 ... 255] = 1};

int Graph[256][256];

const int INF = 1 << 24;

void initGraph() {
	int i, j;
	for (i = 0; i < 256; i++) {
		for (j = 0; j < 256; j++)
			if (i == j)
				Graph[i][j] = 0;
			else
				Graph[i][j] = INF;
	}
}

void printCosts() {
	int i;
	printf("NodeID: %d\n", globalMyID);
	for (i = 0; i < 8; i++) {
		printf("%d ", initCosts[i]);
	}
	printf("\n");
}


int main(int argc, char** argv)
{
	if(argc != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", argv[0]);
		exit(1);
	}
	
	//initialization: get this process's node ID, record what time it is, 
	//and set up our sockaddr_in's for sending to the other nodes.
	globalMyID = atoi(argv[1]);
	int i;
	for(i=0;i<256;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);
		
		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);
	}
	
	//TODO: read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty.
	FILE *finit = fopen(argv[2], "r");
	char buff[32];
	int nbID, nbDist;
	while (!feof(finit)) {
		memset(buff, '\0', sizeof(buff));
		fgets(buff, 32, finit);
		if (buff[0] == '\n' || buff[0] == '\0')
			continue;
		sscanf(buff, "%d %d", &nbID, &nbDist);
		initCosts[nbID] = nbDist;
	}
	fclose(finit);
	initCosts[globalMyID] = 0;

//	printCosts();

	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);	
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}

	initGraph();

	//start threads... feel free to add your own, and to remove the provided ones.
	pthread_t announcerThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);

	pthread_t checkFailureThread;
	pthread_create(&checkFailureThread, 0, checkUpdate, (void*)0);

	flog = fopen(argv[3], "w");
	//good luck, have fun!
	listenForNeighbors(flog);
}
