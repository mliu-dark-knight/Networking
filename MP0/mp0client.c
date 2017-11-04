#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define HOSTNAME "cs438.cs.illinois.edu"
#define PORT "5900"
#define MAXDATASIZE 100
#define OFFSET 12

void send_message(int sockfd, char* message) {
	send(sockfd, message, strlen(message), 0);

}

void recv_message(int sockfd, int print) {
	char buffer[MAXDATASIZE];
	int numbyte = recv(sockfd, buffer, MAXDATASIZE - 1, 0);
	buffer[numbyte] = '\0';
	if (print)
		printf("Received: %s", buffer + OFFSET);
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
	    fprintf(stderr,"usage: client hostname, port, username\n");
	    exit(1);
	}

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(HOSTNAME, PORT, &hints, &servinfo);

	if (status == -1) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		freeaddrinfo(servinfo);
		exit(1);
	}

	struct addrinfo *p;
	int sockfd;
	for (p = servinfo; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd != -1) {
			if (connect(sockfd, p->ai_addr, p->ai_addrlen) != -1)
				break;
			close(sockfd);
		}
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		exit(1);
	}

	char ip[INET6_ADDRSTRLEN];
	if (p->ai_family == AF_INET)
		inet_ntop(p->ai_family, &((struct sockaddr_in*)p->ai_addr)->sin_addr, ip, INET_ADDRSTRLEN);
	else
		inet_ntop(p->ai_family, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, ip, INET6_ADDRSTRLEN);
//	printf("client: connecting to %s\n", ip);

	freeaddrinfo(servinfo);

	char buffer[MAXDATASIZE];
	memset(buffer, 0, MAXDATASIZE * sizeof(char));
	int numbyte;

	char message[MAXDATASIZE];

	send_message(sockfd, "HELO\n");
	recv_message(sockfd, 0);
//	recv_message(sockfd, 1);

	sprintf(message, "USERNAME %s\n", argv[3]);
	send_message(sockfd, message);
	recv_message(sockfd, 0);
//	recv_message(sockfd, 1);

	int c;
	for (c = 0; c < 10; c++) {
		send_message(sockfd, "RECV\n");
		recv_message(sockfd, 1);
	}

	send_message(sockfd, "BYE\n");
	recv_message(sockfd, 0);
//	recv_message(sockfd, 1);
	close(sockfd);
	return 0;
}

