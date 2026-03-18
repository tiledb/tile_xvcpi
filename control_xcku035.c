#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "gpio_io.h"
#include "xilinx_parts.h"
#include "fpga_xcku035.h"

#ifndef MAX_DEVICES
#define MAX_DEVICES 4
#endif

// --- JTAG TAP helpers ---
void jtag_reset() {
    for (int i = 0; i < 5; i++)
        gpio_xfer(1, 1, 0); // TMS high 5 cycles
}

void go_to_shift_dr() {
    gpio_xfer(1, 0, 0);
    gpio_xfer(1, 1, 0);
    gpio_xfer(1, 0, 0);
    gpio_xfer(1, 0, 0);
}

void go_to_shift_ir() {
    gpio_xfer(1, 1, 0);
    gpio_xfer(1, 1, 0);
    gpio_xfer(1, 0, 0);
    gpio_xfer(1, 0, 0);
}

// --- Scan JTAG chain ---
int scan_jtag_chain(uint32_t *idcodes, int max_devices) {
    jtag_reset();
    go_to_shift_dr();

    int count = 0;
    while (count < max_devices) {
        uint32_t id = gpio_xfer(32, 0, 0);
        if (id == 0 || id == 0xFFFFFFFF) break;
        idcodes[count++] = id;
    }

    // Exit DR
    gpio_xfer(1, 1, 0);
    gpio_xfer(1, 0, 0);
    gpio_xfer(1, 0, 0);

    return count;
}

void print_idcodes(uint32_t *idcodes, int n) {
    if (n <= 0) { printf("No devices found.\n"); return; }
    printf("Found %d device(s) on JTAG chain:\n", n);
    for (int i = 0; i < n; i++) {
        uint32_t id = idcodes[i];
        const char *name = xilinx_device_name(id);
        printf("Device %d: IDCODE=0x%08X  Name=%s\n", i, id, name);
    }
}

// --- Shift IR for target device ---
void shift_ir(int device_index, uint8_t instruction, int total_devices) {
    const int ir_len = 6;

    go_to_shift_ir();

    // BYPASS for devices before
    for (int i = 0; i < device_index; i++)
        gpio_xfer(ir_len, 0x3F, 0x3F);

    // Target instruction
    gpio_xfer(ir_len, instruction & 0x3F, instruction & 0x3F);

    // BYPASS for devices after
    for (int i = device_index + 1; i < total_devices; i++)
        gpio_xfer(ir_len, 0x3F, 0x3F);
}

// --- Read DR (32-bit chunk) ---
uint32_t read_dr32() {
    return gpio_xfer(32, 0, 0);
}

// --- Read 96-bit DNA via CFG_OUT ---
void read_dna96(int device_index, uint32_t dna[3]) {
    const int total_devices = MAX_DEVICES;

    jtag_reset();
    shift_ir(device_index, 0x04, total_devices); // CFG_OUT

    go_to_shift_dr();
    dna[0] = read_dr32(); // LSB
    dna[1] = read_dr32();
    dna[2] = read_dr32(); // MSB
}

void print_dna96(int device_index) {
    uint32_t dna[3];
    read_dna96(device_index, dna);
    printf("DNA (96-bit): 0x%08X%08X%08X\n", dna[2], dna[1], dna[0]);
}

// --- Read 32-bit register by IR ---
uint32_t read_register32(int device_index, uint8_t ir) {
    const int total_devices = MAX_DEVICES;
    jtag_reset();
    shift_ir(device_index, ir, total_devices);
    go_to_shift_dr();
    return read_dr32();
}

// --- Decode STATUS bits ---
void decode_status(uint32_t val) {
    printf(" STATUS decode:\n");
    printf("   INIT:      %d\n", (val >> 0) & 1);
    printf("   DONE:      %d\n", (val >> 1) & 1);
    printf("   RDY:       %d\n", (val >> 2) & 1);
    printf("   ERR:       %d\n", (val >> 3) & 1);
    printf("   CRC_ERR:   %d\n", (val >> 4) & 1);
    printf("   CONF_DONE: %d\n", (val >> 5) & 1);
}

// --- Decode CTRL bits ---
void decode_ctrl(uint32_t val) {
    printf(" CTRL decode:\n");
    printf("   GREST:     %d\n", (val >> 0) & 1);
    printf("   GWE:       %d\n", (val >> 1) & 1);
    printf("   NSE:       %d\n", (val >> 2) & 1);
    printf("   PGM:       %d\n", (val >> 3) & 1);
    printf("   INIT_EN:   %d\n", (val >> 4) & 1);
    printf("   DONE_EN:   %d\n", (val >> 5) & 1);
    printf("   CRC_EN:    %d\n", (val >> 6) & 1);
}

// --- Print meaningful XCKU035 registers ---
void print_xcku035_registers(int device_index) {
    printf("\nReading XCKU035 registers (UG570-compliant)...\n");

    // IDCODE
    uint32_t idcode = read_register32(device_index, 0x09);
    printf("IDCODE     (0x09): 0x%08X\n", idcode);

    // USERCODE
    uint32_t usercode = read_register32(device_index, 0x08);
    printf("USERCODE   (0x08): 0x%08X\n", usercode);

    // DNA
    print_dna96(device_index);

    // CTRL
    uint32_t ctrl = read_register32(device_index, 0x05);
    printf("CTRL       (0x05): 0x%08X\n", ctrl);
    decode_ctrl(ctrl);

    // STATUS
    uint32_t status = read_register32(device_index, 0x06);
    printf("STATUS     (0x06): 0x%08X\n", status);
    decode_status(status);
}

// --- Main ---
int main(int argc, char **argv) {
    int c;
    int side_param = 1;
    int minidrawer_param = 1;

    while ((c = getopt(argc, argv, "s:m:h")) != -1) {
        switch(c) {
            case 's': side_param = atoi(optarg); break;
            case 'm': minidrawer_param = atoi(optarg); break;
            case 'h':
            default:
                printf("Usage: %s [-s side] [-m minidrawer]\n", argv[0]);
                return 0;
        }
    }

    if (gpio_init() < 1) {
        fprintf(stderr, "GPIO initialization failed.\n");
        return -1;
    }

    gpioWrite(md_enable_gpio_lut[minidrawer_param-1], 0);
    gpioWrite(side_gpio, side_param == 2 ? 1 : 0);

    uint32_t idcodes[MAX_DEVICES];
    int num_devices = scan_jtag_chain(idcodes, MAX_DEVICES);
    print_idcodes(idcodes, num_devices);

    int xcku_index = -1;
    for (int i = 0; i < num_devices; i++) {
        if (strcmp(xilinx_device_name(idcodes[i]), "XCKU035") == 0) {
            xcku_index = i;
            break;
        }
    }

    if (xcku_index == -1) {
        printf("XCKU035 not found in chain.\n");
        return 1;
    }

    print_xcku035_registers(xcku_index);
    return 0;
}
