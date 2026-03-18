#ifndef XILINX_IDCODES_H
#define XILINX_IDCODES_H

#include <stdint.h>

typedef struct {
    uint32_t idcode;
    const char *name;
} xilinx_device_t;

static const xilinx_device_t xilinx_devices[] = {
    /* Spartan‑3 */
    {0x03074093, "XC3S200"},  {0x03094093, "XC3S500E"},

    /* Spartan‑6 */
    {0x02648093, "XC6SLX4"},   {0x02681093, "XC6SLX9"},
    {0x026C8093, "XC6SLX16"},  {0x02708093, "XC6SLX25"},
    {0x02748093, "XC6SLX25T"}, {0x027C8093, "XC6SLX45"},
    {0x02808093, "XC6SLX45T"}, {0x028E8093, "XC6SLX75"},
    {0x02908093, "XC6SLX75T"}, {0x029A4093, "XC6SLX100"},
    {0x02A08093, "XC6SLX100T"},{0x02BC8093, "XC6SLX150"},
    {0x02C08093, "XC6SLX150T"},{0x02DE8093, "XC6SLX200"},
    {0x02E08093, "XC6SLX200T"},

    /* Spartan‑7 */
    {0x04E81093, "XC7S15"},  {0x04E82093, "XC7S25"},
    {0x04E80093, "XC7S50"},

    /* Artix‑7 */
    {0x0362D093, "XC7A35T"}, {0x0362E093, "XC7A50T"},
    {0x03650093, "XC7A100T"},{0x03688093, "XC7A200T"},
    {0x0368A093, "XC7A75T"},

    /* Kintex‑7 */
    {0x0480B093, "XC7K70T"},  {0x0480C093, "XC7K160T"},
    {0x0482E093, "XC7K325T"},{0x04830093, "XC7K410T"},
    {0x04831093, "XC7K420T"},{0x04834093, "XC7K480T"},

    /* Virtex‑7 */
    {0x04948093, "XC7V585T"},{0x0496F093, "XC7V2000T"},
    {0x049C9093, "XC7VX330T"},{0x049CB093, "XC7VX415T"},
    {0x049CE093, "XC7VX485T"},{0x049D9093, "XC7VX550T"},
    {0x049E9093, "XC7VX690T"},{0x049F9093, "XC7VX980T"},
    {0x04A09093, "XC7VX1140T"},{0x04A18093, "XC7VH580T"},
    {0x04A38093, "XC7VH870T"},

    /* Zynq‑7000 */
    {0x03841093, "XC7Z007"},{0x03842093, "XC7Z010"},
    {0x0384A093, "XC7Z015"},{0x03846093, "XC7Z020"},
    {0x0384F093, "XC7Z030"},{0x03845093, "XC7Z035"},
    {0x0384E093, "XC7Z045"},{0x0384D093, "XC7Z100"},

    /* Kintex UltraScale */
    {0x03823093, "XCKU035"}, {0x03824093, "XCKU040"},
    {0x03919093, "XCKU060"}, {0x0380F093, "XCKU085"},
    {0x03844093, "XCKU095"}, {0x0390D093, "XCKU115"},
    {0x03923093, "XCKU125"}, {0x0392F093, "XCKU155"},
    {0x03948093, "XCKU190"}, {0x0399F093, "XCKU200"},

    /* Virtex UltraScale */
    {0x03939093, "XCVU065"},{0x03843093, "XCVU080"},
    {0x03842093, "XCVU095"},{0x0392D093, "XCVU125"},
    {0x03933093, "XCVU160"},{0x03911093, "XCVU190"},
    {0x0396D093, "XCVU440"},

    /* Kintex UltraScale+ */
    {0x04A63093, "XCKU3P"},{0x04A62093, "XCKU5P"},
    {0x0484A093, "XCKU9P"},{0x04A4E093, "XCKU11P"},
    {0x04A52093, "XCKU13P"},{0x04A56093, "XCKU15P"},

    /* Virtex UltraScale+ */
    {0x04B39093, "XCVU3P"},{0x04B2B093, "XCVU5P"},
    {0x04B29093, "XCVU7P"},{0x04B31093, "XCVU9P"},
    {0x04B49093, "XCVU11P"},{0x04B51093, "XCVU13P"},
    {0x04B6B093, "XCVU31P"},{0x04B69093, "XCVU33P"},
    {0x04B71093, "XCVU35P"},{0x04B79093, "XCVU37P"},

    /* Xilinx PROM / CPLD */
    {0x05024093, "XCF01S"},{0x05025093, "XCF02S"},
    {0x05026093, "XCF04S"},{0x05027093, "XCF08P"},
    {0x05028093, "XCF16P"},{0x05029093, "XCF32P"},

    /* End */
    {0, NULL}
};
// Lookup by IDCODE
static inline const char *xilinx_device_name(uint32_t idcode) {
    for (int i = 0; xilinx_devices[i].name != NULL; i++) {
        if ((xilinx_devices[i].idcode & 0x0FFFFFFE) == (idcode & 0x0FFFFFFE))
            return xilinx_devices[i].name;
    }
    return "Unknown Xilinx Device";
}

#endif
