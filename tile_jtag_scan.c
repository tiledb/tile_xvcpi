#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "gpio_io.h"
#include "xilinx_parts.h"  // <--- include the header

// JTAG TAP functions
void jtag_reset() {
    for (int i = 0; i < 5; i++) gpio_xfer(1, 1, 0);
}

void go_to_shift_dr() {
    gpio_xfer(1, 0, 0); // Run-Test/Idle
    gpio_xfer(1, 1, 0); // Select-DR-Scan
    gpio_xfer(1, 0, 0); // Capture-DR
    gpio_xfer(1, 0, 0); // Shift-DR
}

// Scan chain
int scan_jtag_chain(uint32_t *idcodes, int max_devices) {
    jtag_reset();
    go_to_shift_dr();

    int count = 0;
    while (count < max_devices) {
        uint32_t id = gpio_xfer(32, 0, 0);
        if (id == 0 || id == 0xFFFFFFFF) break;
        idcodes[count++] = id;
    }

    gpio_xfer(1, 1, 0);
    gpio_xfer(1, 0, 0);
    gpio_xfer(1, 0, 0);

    return count;
}

// Print IDCODEs from a JTAG scan
void print_idcodes(uint32_t *idcodes, int n) {
    if (n <= 0) {
        printf("No devices found on the JTAG chain.\n");
        return;
    }

    printf("Found %d device(s) on the JTAG chain:\n", n);
    printf("-------------------------------------------------------------\n");
    printf("| # | IDCODE     | Device Name       | Version | Manufacturer |\n");
    printf("-------------------------------------------------------------\n");

    for (int i = 0; i < n; i++) {
        uint32_t id = idcodes[i];

        // Extract fields from IDCODE
        uint8_t version = (id >> 28) & 0xF;           // bits 31:28
        uint16_t part    = (id >> 12) & 0xFFFF;      // bits 27:12
        uint16_t manuf   = (id >> 1) & 0x7FF;        // bits 11:1
        uint8_t lsb      = id & 0x1;                 // bit 0 (should always be 1)

        const char *dev_name = xilinx_device_name(id);

        printf("| %d | 0x%08X | %-16s | 0x%X      | 0x%03X        |\n",
               i, id, dev_name, version, manuf);
    }

    printf("-------------------------------------------------------------\n");
}

int main(int argc, char **argv) {
    int c;
    int max_devices = 4;
    int side_param = 1;
    int minidrawer_param = 1;

    while ((c = getopt(argc, argv, "s:m:n:h")) != -1) {
        switch (c) {
            case 's': side_param = atoi(optarg); break;
            case 'm': minidrawer_param = atoi(optarg); break;
            case 'n': max_devices = atoi(optarg); break;
            case 'h':
            default:
                printf("Usage: %s [-s side] [-m minidrawer] [-n max_devices]\n", argv[0]);
                return 0;
        }
    }

    if (gpio_init() < 1) {
        fprintf(stderr, "Failed to initialize GPIO\n");
        return -1;
    }

    gpioWrite(md_enable_gpio_lut[minidrawer_param-1], 0);
    if (side_param == 1) gpioWrite(side_gpio, 0);
    else if (side_param == 2) gpioWrite(side_gpio, 1);

    uint32_t idcodes[max_devices];
    int num = scan_jtag_chain(idcodes, max_devices);
    print_idcodes(idcodes, num);

    return 0;
}
