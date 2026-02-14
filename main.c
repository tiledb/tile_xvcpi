#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "gpio_io.h"
#include "tcp_server.h"

/* External handle_data function from your original code */
extern int handle_data(int fd);

int verbose = 0;

int main(int argc, char **argv) {
    int c;
    char side_char = 'a';
    int p = -1;
    int tcp_port = 2542;

    while ((c = getopt(argc, argv, "?vj:p:m:s:")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 'p':
                p = atoi(optarg);
                fprintf(stderr, "TCP port parameter: %d\n", p);
                break;
            case 'j':
                jtag_delay = atoi(optarg);
                fprintf(stderr, "Setting jtag_delay: %d\n", jtag_delay);
                break;
            case 'm':
                minidrawer = atoi(optarg);
                if (p == 0) {
                    tcp_port = 2542;
                } else if (p < 0) {
                    tcp_port = 2542 - 1 + minidrawer;
                } else {
                    tcp_port = p;
                }
                fprintf(stderr, "Setting tcp_port from parameter: %d\n", tcp_port);
                fprintf(stderr, "Selecting minidrawer: %d\n", minidrawer);
                break;
            case 's':
                side = atoi(optarg);
                break;
            case '?':
                fprintf(stderr, "help\n");
                fprintf(stderr, "usage: %s [-v] [-p select tcp port] [-m minidrawer 1-4] [-s side 1=a 2=b] [-j jtag delay]\n", *argv);
                return 1;
        }
    }

    /* Initialize GPIOs */
    if (gpio_init() < 1) {
        fprintf(stderr, "Failed in gpio_init()\n");
        return -1;
    }

    gpioWrite(md_enable_gpio_lut[minidrawer-1], 0);

    switch (side) {
        case 0:
            fprintf(stderr, "Broadcasting! (side: %d)\n", side);
            side_char = '*';
            gpioWrite(side_gpio, 0);
            break;
        case 1:
            fprintf(stderr, "Selecting side: %d (a)\n", side);
            side_char = 'a';
            gpioWrite(side_gpio, 0);
            break;
        case 2:
            fprintf(stderr, "Selecting side: %d (b)\n", side);
            side_char = 'b';
            gpioWrite(side_gpio, 1);
            break;
    }

    fprintf(stderr, "XVCPI initialized in tcp_port: %d, at speed delay factor: %d, with pins:\n", tcp_port, jtag_delay);
    fprintf(stderr, "Minidrawer: %d -> Side %c:\n", minidrawer, side_char);
    fprintf(stderr, "tcp_port:[%d]  tck:gpio[%d], tms:gpio[%d], tdi:gpio[%d], tdo:gpio[%d]\n",
            tcp_port, tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1],
            tdi_gpio_lut[minidrawer-1], tdo_gpio_lut[minidrawer-1]);

    /* Start TCP server */
    return start_tcp_server(tcp_port, handle_data, verbose);
}
