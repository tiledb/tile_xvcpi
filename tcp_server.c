#include "tcp_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>

int start_tcp_server(int tcp_port, int (*handle_data_callback)(int fd), int verbose)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int i = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);

    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(tcp_port);
    address.sin_family = AF_INET;

    if (bind(s, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("bind"); return 1; }
    if (listen(s, 0) < 0) { perror("listen"); return 1; }

    fd_set conn;
    int maxfd = s;
    FD_ZERO(&conn);
    FD_SET(s, &conn);

    while (1) {
        fd_set read = conn, except = conn;
        int fd;

        if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
            perror("select");
            break;
        }

        for (fd = 0; fd <= maxfd; ++fd) {
            if (FD_ISSET(fd, &read)) {
                if (fd == s) {
                    int newfd;
                    socklen_t nsize = sizeof(address);
                    newfd = accept(s, (struct sockaddr *)&address, &nsize);
                    if (verbose) printf("connection accepted - fd %d\n", newfd);
                    if (newfd < 0) perror("accept");
                    else {
                        int flag = 1;
                        setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
                        if (newfd > maxfd) maxfd = newfd;
                        FD_SET(newfd, &conn);
                    }
                } else if (handle_data_callback(fd)) {
                    if (verbose) printf("connection closed - fd %d\n", fd);
                    close(fd);
                    FD_CLR(fd, &conn);
                }
            } else if (FD_ISSET(fd, &except)) {
                if (verbose) printf("connection aborted - fd %d\n", fd);
                close(fd);
                FD_CLR(fd, &conn);
                if (fd == s) break;
            }
        }
    }
    return 0;
}
