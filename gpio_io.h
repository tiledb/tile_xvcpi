#ifndef GPIO_IO_H
#define GPIO_IO_H

#include <stdint.h>

#define ERROR_JTAG_INIT_FAILED -1
#define ERROR_OK 1

int gpio_init(void);
uint32_t gpio_xfer(int n, uint32_t tms, uint32_t tdi);
int gpio_read(int the_tdo_gpio);
void gpioWrite(int the_gpio, int the_value);
void gpio_write(int the_tck_gpio, int the_tms_gpio, int the_tdi_gpio, int tck, int tms, int tdi);

/* GPIO configuration */
extern int tck_gpio_lut[4];
extern int tms_gpio_lut[4];
extern int tdi_gpio_lut[4];
extern int tdo_gpio_lut[4];
extern int md_enable_gpio_lut[4];
extern int side_gpio;
extern int minidrawer;
extern int side;
extern unsigned int jtag_delay;

#endif
