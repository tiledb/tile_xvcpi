#include "gpio_io.h"
#include "xvc_protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* use external verbose variable */
extern int verbose;

int sread(int fd, void *target, int len) {
    unsigned char *t = target;
    while (len) {
        int r = read(fd, t, len);
        if (r <= 0)
            return r;
        t += r;
        len -= r;
    }
    return 1;
}

int handle_data(int fd) {
    const char xvcInfo[] = "xvcServer_v1.0:2048\n";

    do {
        char cmd[16];
        unsigned char buffer[2048], result[1024];
        memset(cmd, 0, 16);

        if (sread(fd, cmd, 2) != 1)
            return 1;

        if (memcmp(cmd, "ge", 2) == 0) {
            if (sread(fd, cmd, 6) != 1) return 1;
            memcpy(result, xvcInfo, strlen(xvcInfo));
            if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) return 1;
            if (verbose) {
                printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
                printf("\t Replied with %s\n", xvcInfo);
            }
            break;
        } else if (memcmp(cmd, "se", 2) == 0) {
            if (sread(fd, cmd, 9) != 1) return 1;
            memcpy(result, cmd + 5, 4);
            if (write(fd, result, 4) != 4) return 1;
            if (verbose) {
                printf("%u : Received command: 'settck'\n", (int)time(NULL));
                printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
            }
            break;
        } else if (memcmp(cmd, "sh", 2) == 0) {
            if (sread(fd, cmd, 4) != 1) return 1;
            if (verbose) printf("%u : Received command: 'shift'\n", (int)time(NULL));
        } else {
            fprintf(stderr, "invalid cmd '%s'\n", cmd);
            return 1;
        }

        int len;
        if (sread(fd, &len, 4) != 1) {
            fprintf(stderr, "reading length failed\n");
            return 1;
        }

        int nr_bytes = (len + 7) / 8;
        if (nr_bytes * 2 > sizeof(buffer)) {
            fprintf(stderr, "buffer size exceeded\n");
            return 1;
        }

        if (sread(fd, buffer, nr_bytes * 2) != 1) {
            fprintf(stderr, "reading data failed\n");
            return 1;
        }
        memset(result, 0, nr_bytes);

        if (verbose) {
            printf("\tNumber of Bits  : %d\n", len);
            printf("\tNumber of Bytes : %d \n", nr_bytes);
            printf("\n");
        }

        gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1],
                   tdi_gpio_lut[minidrawer-1], 0, 1, 1);

        int bytesLeft = nr_bytes;
        int bitsLeft = len;
        int byteIndex = 0;
        uint32_t tdi, tms, tdo;

        while (bytesLeft > 0) {
            tms = 0;
            tdi = 0;
            tdo = 0;
            if (bytesLeft >= 4) {
                memcpy(&tms, &buffer[byteIndex], 4);
                memcpy(&tdi, &buffer[byteIndex + nr_bytes], 4);
                tdo = gpio_xfer(32, tms, tdi);
                memcpy(&result[byteIndex], &tdo, 4);
                bytesLeft -= 4;
                bitsLeft -= 32;
                byteIndex += 4;
                if (verbose) {
                    printf("LEN : 0x%08x\n", 32);
                    printf("TMS : 0x%08x\n", tms);
                    printf("TDI : 0x%08x\n", tdi);
                    printf("TDO : 0x%08x\n", tdo);
                }
            } else {
                memcpy(&tms, &buffer[byteIndex], bytesLeft);
                memcpy(&tdi, &buffer[byteIndex + nr_bytes], bytesLeft);
                tdo = gpio_xfer(bitsLeft, tms, tdi);
                memcpy(&result[byteIndex], &tdo, bytesLeft);
                bytesLeft = 0;
                if (verbose) {
                    printf("LEN : 0x%08x\n", bitsLeft);
                    printf("TMS : 0x%08x\n", tms);
                    printf("TDI : 0x%08x\n", tdi);
                    printf("TDO : 0x%08x\n", tdo);
                }
                break;
            }
        }

        gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1],
                   tdi_gpio_lut[minidrawer-1], 0, 1, 0);

        if (write(fd, result, nr_bytes) != nr_bytes) {
            perror("write");
            return 1;
        }

    } while (1);

    return 0;
}
