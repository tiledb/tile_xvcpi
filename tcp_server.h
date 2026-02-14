#ifndef TCP_SERVER_H
#define TCP_SERVER_H

int start_tcp_server(int tcp_port, int (*handle_data_callback)(int fd), int verbose);

#endif
