#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BACKLOG 10
#define MAXDATASIZE 1024


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr,"usage: port\n");
		exit(1);
	}

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = getaddrinfo(NULL, argv[1], &hints, &servinfo);
	if (status != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		freeaddrinfo(servinfo);
		return 1;
	}

	struct addrinfo *p;
	int sockfd;
	int yes = 1;
	for(p = servinfo; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd != -1)
			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != -1)
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(int)) != -1)
                    if (bind(sockfd, p->ai_addr, p->ai_addrlen) != -1)
                        break;
		close(sockfd);
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		freeaddrinfo(servinfo);
		return 1;
	}

	freeaddrinfo(servinfo);

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	int accept_fd;
	struct sockaddr_storage client_addr;
	socklen_t addrlen;
	while (1) {
		addrlen = sizeof client_addr;
		accept_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
		if (accept_fd == -1) {
			perror("accept");
			continue;
		}

//		char ip[INET6_ADDRSTRLEN];
//		if (p->ai_family == AF_INET)
//			inet_ntop(p->ai_family, &((struct sockaddr_in*)p->ai_addr)->sin_addr, ip, INET_ADDRSTRLEN);
//		else
//			inet_ntop(p->ai_family, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, ip, INET6_ADDRSTRLEN);
//		printf("server: got connection from %s\n", ip);

		char *prefix = "GET /";
		if (!fork()) {
			close(sockfd);
			char request[MAXDATASIZE];
			char response[MAXDATASIZE];
			int numbyte = recv(accept_fd, request, MAXDATASIZE - 1, 0);
			request[numbyte] = '\0';
			printf("%s", request);
			if (numbyte > 0) {
				int offset = 5;
				if (memcmp(request, prefix, offset) == 0) {
					int idx;
					for (idx = offset; idx < numbyte; idx++) {
						if (request[idx] == ' ' || (request[idx] == '\r' && request[idx + 1] == '\n'))
							break;
					}
					char path[MAXDATASIZE];
					memcpy(path, request + offset, idx - offset);
					path[idx - offset] = '\0';

                    struct stat statbuf;
                    stat(path, &statbuf);
					FILE *fp = fopen(path, "rb");
					if (fp == NULL || S_ISDIR(statbuf.st_mode) != 0) {
						sprintf(response, "HTTP/1.0 404 Not Found\r\n\r\n");
						send(accept_fd, response, strlen(response), 0);
					}
					else {
						sprintf(response, "HTTP/1.0 200 OK\r\n\r\n");
						send(accept_fd, response, strlen(response), 0);
						while (!feof(fp)) {
							int n_read = fread(response, 1, MAXDATASIZE, fp);
                            send(accept_fd, response, n_read, 0);
							memset(response, '\0', MAXDATASIZE * sizeof(char));
						}
					}
					fclose(fp);
				}
				else {
					sprintf(response, "HTTP/1.0 400 Bad Request\r\n\r\n");
					send(accept_fd, response, strlen(response), 0);
				}
			}
			close(accept_fd);
			exit(0);
		}
		close(accept_fd);
	}

	return 0;
}

