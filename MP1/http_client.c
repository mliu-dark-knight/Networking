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

#define MAXDATASIZE 1024

typedef struct URL {
	char hostname[64];
	char port[5];
	char path[1024];
} URL;

URL *parse_url(char *url) {
	URL *Url = malloc(sizeof(URL));
	memset(Url, 0, sizeof(URL));
	int port = -1;
	sscanf(url, "http://%99[A-Za-z0-9.]:%99d/%99[^\n]", Url->hostname, &port, Url->path);
	if (port == -1) {
		sscanf(url, "http://%99[A-Za-z0-9.]/%99[^\n]", Url->hostname, Url->path);
		port = 80;
	}
	sprintf(Url->port, "%d", port);
	return Url;
}

void send_request(char* url, FILE *fp) {
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	URL *Url = parse_url(url);
//    printf("hostname: %s\n", Url->hostname);
//    printf("port: %s\n", Url->port);
//    printf("path: %s\n", Url->path);

	int status = getaddrinfo(Url->hostname, Url->port, &hints, &servinfo);

	if (status == -1) {
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		freeaddrinfo(servinfo);
        free(Url);
        return;
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
        freeaddrinfo(servinfo);
        free(Url);
        return;
	}

	char ip[INET6_ADDRSTRLEN];
	if (p->ai_family == AF_INET)
		inet_ntop(p->ai_family, &((struct sockaddr_in*)p->ai_addr)->sin_addr, ip, INET_ADDRSTRLEN);
	else
		inet_ntop(p->ai_family, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, ip, INET6_ADDRSTRLEN);
	printf("client: connecting to %s\n", ip);

	freeaddrinfo(servinfo);

	char message[MAXDATASIZE];
	memset(message, 0, MAXDATASIZE * sizeof(char));
	sprintf(message, "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n", Url->path, Url->hostname);
	send(sockfd, message, strlen(message), 0);

	free(Url);


	char response[MAXDATASIZE];
	char *prefix_rd = "301 Moved Permanently\r\nLocation: ";
	int offset = 42;
	int write = 0;
	while (1) {
		int numbyte = recv(sockfd, response, MAXDATASIZE - 1, 0);
		response[numbyte] = '\0';
//		printf("%s", response);
		if (write)
			fwrite(response, 1, numbyte, fp);
		else {
			int idx;
			if (memcmp(response + 9, prefix_rd, strlen(prefix_rd)) == 0) {
				close(sockfd);
				char new_url[MAXDATASIZE];

				for (idx = offset; idx < MAXDATASIZE - 1; idx++) {
					if (response[idx] == '\r' && response[idx + 1] == '\n')
						break;
				}
                memset(new_url, '\0', MAXDATASIZE);
				memcpy(new_url, response + offset, idx - offset);
//				printf("new url: %s\n", new_url);
				send_request(new_url, fp);
				return;
			}

			else {
				for (idx = 0; idx < MAXDATASIZE - 3; idx++) {
					if (response[idx] == '\r' && response[idx + 1] == '\n' && response[idx + 2] == '\r' &&
						response[idx + 3] == '\n') {
						write = 1;
						fwrite(response + idx + 4, 1, numbyte - idx - 4, fp);
					}
				}
			}
		}

		if (numbyte == 0)
			break;
	}

	close(sockfd);
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr,"usage: url\n");
		exit(1);
	}

	FILE *fp = fopen("output", "wb");
	send_request(argv[1], fp);
	fclose(fp);
	return 0;
}
