#ifndef FPGA_XCKU035_H
#define FPGA_XCKU035_H

#include <stdint.h>

/*
 * XCKU035 JTAG register list for control_xcku035
 * IR: 6-bit instruction register code
 * Name: human-readable register name
 * Desc: brief description
 */

typedef struct {
    uint32_t ir;       // instruction code (6-bit)
    const char *name;  // register name
    const char *desc;  // short description
} xcku035_reg_t;

static const xcku035_reg_t xcku035_regs[] = {
    {0x3F, "BYPASS",  "Bypass register (1-bit)"},
    {0x26, "EXTEST",  "Boundary scan test"},
    {0x01, "SAMPLE",  "Sample/Preload"},
    {0x02, "USER1",   "User register 1"},
    {0x03, "USER2",   "User register 2"},
    {0x22, "USER3",   "User register 3"},
    {0x23, "USER4",   "User register 4"},
    {0x08, "USERCODE","User code register"},
    {0x09, "IDCODE",  "Device IDCODE"},
    {0x0A, "HIGHZ_IO","Disable user I/O"},
    {0x05, "CFG_IN",  "Configuration bus write"},
    {0x04, "CFG_OUT", "Configuration bus read"},
    {0x0B, "JPROGRAM","PROGRAM_B equivalent"},
    {0x0C, "JSTART",  "Startup sequence"},
    {0x0D, "JSHUTDOWN","Shutdown sequence"},
    {0x37, "SYSMON_DRP","System Monitor DRP"},
    {0, NULL, NULL}
};

#endif
