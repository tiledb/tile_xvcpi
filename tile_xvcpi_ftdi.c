#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <libusb-1.0/libusb.h>

#include "tcp_server.h"

static libusb_device_handle *ftdi = NULL;
static libusb_device **dev_list = NULL;
static size_t dev_count = 0;

static uint32_t tck_ns = 1000000;
static int verbose = 0;

/* ---------------- Detect FTDI devices ---------------- */
void list_ftdi_devices()
{
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    dev_count = 0;

    if (cnt < 0) {
        fprintf(stderr, "Error enumerating USB devices\n");
        return;
    }

    printf("FTDI devices found:\n");
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        libusb_get_device_descriptor(dev_list[i], &desc);
        if (desc.idVendor == 0x0403) {
            printf("[%zu] VID:PID=%04x:%04x, bus=%03u, device=%03u\n",
                   dev_count, desc.idVendor, desc.idProduct,
                   libusb_get_bus_number(dev_list[i]),
                   libusb_get_device_address(dev_list[i]));
            dev_count++;
        }
    }
    if (dev_count == 0)
        printf("No FTDI devices found.\n");
}

/* ---------------- Open selected FTDI device ---------------- */
int ftdi_init_index(size_t index)
{
    if (dev_count == 0 || index >= dev_count) {
        fprintf(stderr, "Invalid FTDI device index %zu\n", index);
        return -1;
    }

    struct libusb_device_descriptor desc;
    libusb_device *dev = dev_list[index];
    libusb_get_device_descriptor(dev, &desc);

    ftdi = NULL;

    // Open device
    int r = libusb_open(dev, &ftdi);
    if (r != 0 || !ftdi) {
        fprintf(stderr, "Failed to open FTDI device %zu: %s\n", index, libusb_error_name(r));
        return -1;
    }

    // Detach kernel driver if needed
    if (libusb_kernel_driver_active(ftdi, 0) == 1)
        libusb_detach_kernel_driver(ftdi, 0);

    if (libusb_claim_interface(ftdi, 0) != 0) {
        fprintf(stderr, "Failed to claim interface\n");
        return -1;
    }

    // Reset and enable MPSSE
    libusb_control_transfer(ftdi, 0x40, 0x00, 0, 0, NULL, 0, 1000);
    libusb_control_transfer(ftdi, 0x40, 0x0B, 0, 0, NULL, 0, 1000);

    unsigned char cmd = 0x85; // disable loopback
    libusb_bulk_transfer(ftdi, 0x02, &cmd, 1, NULL, 1000);

    printf("Opened FTDI device %zu VID:PID=%04x:%04x in MPSSE mode\n",
           index, desc.idVendor, desc.idProduct);

    return 0;
}

/* ---------------- JTAG SHIFT ---------------- */
void ftdi_jtag_shift(uint32_t nbits,
                     uint8_t *tms,
                     uint8_t *tdi,
                     uint8_t *tdo)
{
    uint32_t nbytes = nbits / 8;
    uint32_t rembits = nbits % 8;
    unsigned char cmd[3];
    int transferred;

    if (nbytes > 0) {
        cmd[0] = 0x39; // clock bytes in/out MSB first
        cmd[1] = (nbytes - 1) & 0xFF;
        cmd[2] = ((nbytes - 1) >> 8) & 0xFF;
        libusb_bulk_transfer(ftdi, 0x02, cmd, 3, &transferred, 1000);
        libusb_bulk_transfer(ftdi, 0x02, tdi, nbytes, &transferred, 1000);
        libusb_bulk_transfer(ftdi, 0x81, tdo, nbytes, &transferred, 1000);
    }

    if (rembits) {
        cmd[0] = 0x3B; // clock bits in/out
        cmd[1] = rembits - 1;
        cmd[2] = tdi[nbytes];
        libusb_bulk_transfer(ftdi, 0x02, cmd, 3, &transferred, 1000);
        libusb_bulk_transfer(ftdi, 0x81, &tdo[nbytes], 1, &transferred, 1000);
    }
}

/* ---------------- XVC HANDLER ---------------- */
int handle_data(int fd)
{
    unsigned char buf[8192];
    int len = read(fd, buf, sizeof(buf));
    if (len <= 0) return -1;

    if (!memcmp(buf, "getinfo:", 8)) {
        const char *resp = "xvcServer_v1.0:1024\n";
        write(fd, resp, strlen(resp));
        return 0;
    }

    if (!memcmp(buf, "settck:", 7)) {
        tck_ns = *(uint32_t *)(buf + 7);
        write(fd, buf + 7, 4);
        return 0;
    }

    if (!memcmp(buf, "shift:", 6)) {
        uint32_t nbits = *(uint32_t *)(buf + 6);
        uint32_t nbytes = (nbits + 7) / 8;
        uint8_t *tms = buf + 10;
        uint8_t *tdi = tms + nbytes;
        uint8_t *tdo = malloc(nbytes);
        if (!tdo) return -1;
        ftdi_jtag_shift(nbits, tms, tdi, tdo);
        write(fd, tdo, nbytes);
        free(tdo);
        return 0;
    }

    return 0;
}

/* ---------------- MAIN ---------------- */
/* ---------------- MAIN ---------------- */
int main(int argc, char **argv)
{
    int tcp_port = 2542;
    size_t ftdi_index = 0; // default first device
    int verbose = 0;
    int list_only = 0;

    int c;
    while ((c = getopt(argc, argv, "vp:d:l")) != -1) {
        switch (c) {
            case 'v':
                verbose = 1;
                break;
            case 'p':
                tcp_port = atoi(optarg);
                break;
            case 'd':
                ftdi_index = atoi(optarg);
                break;
            case 'l':
                list_only = 1;
                break;
            default:
                fprintf(stderr, "usage: %s [-v] [-p tcp_port] [-d ftdi_index] [-l]\n", argv[0]);
                return 1;
        }
    }

    libusb_init(NULL);
    list_ftdi_devices();

    if (list_only) {
        printf("Use -d <index> to select a device.\n");
        libusb_free_device_list(dev_list, 1);
        return 0;
    }

    if (ftdi_init_index(ftdi_index) < 0) {
        fprintf(stderr, "Failed to open FTDI device\n");
        libusb_free_device_list(dev_list, 1);
        return -1;
    }

    fprintf(stderr, "Starting XVC FTDI server on port %d\n", tcp_port);
    int ret = start_tcp_server(tcp_port, handle_data, verbose);

    libusb_free_device_list(dev_list, 1);
    return ret;
}