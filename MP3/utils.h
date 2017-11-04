//
// Created by victor on 4/6/17.
//

#ifndef CS438_UTILS_H
#define CS438_UTILS_H

#include <cstddef>
#include <stdio.h>
#include <stdint.h>
#include <time.h>


typedef struct header {
	char command[4];
	uint64_t length;
	struct timespec time;
} Header;

const int MAX_PKT_SIZE = 1472;
const int MAX_DATA_SIZE = MAX_PKT_SIZE - sizeof(Header);

void sendToAll(const void *buf, size_t len, struct sockaddr *addr, int socket_fd);

#endif