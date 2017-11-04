//
// Created by victor on 4/6/17.
//

#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>
#include <iostream>
#include <cstring>

using namespace std;


void sendToAll(const void *buf, size_t len, struct sockaddr * addr, int socket_fd) {
    while(true) {
        ssize_t retval = sendto(socket_fd, buf, len, 0, addr, sizeof(sockaddr_in));
        if (retval == (ssize_t) len) {
            break;
        } else if (retval == -1) {
            if ((errno != EINTR)) {
                cout << "[SendToAllError]: " << strerror(errno);
                break;
            }
        }
    }
    return;
}

