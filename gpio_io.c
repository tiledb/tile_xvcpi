#include "gpio_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define BCM2711_PERI_BASE        0xFE000000
#define GPIO_BASE                (BCM2711_PERI_BASE + 0x200000) /* GPIO controller */
#define BLOCK_SIZE (4*1024)

int mem_fd;
void *gpio_map;
volatile unsigned *gpio;

#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)
#define GPIO_CLR *(gpio+10)
#define GET_GPIO(g) (*(gpio+13)&(1<<g))
#define GPIO_PULL *(gpio+37)
#define GPIO_PULLCLK0 *(gpio+38)

int tck_gpio_lut[4] = {4,11,25,6};
int tms_gpio_lut[4] = {18,22,16,13};
int tdi_gpio_lut[4] = {2,10,23,0};
int tdo_gpio_lut[4] = {3,9,24,5};
int md_enable_gpio_lut[4] = {17,7,26,1};

int side_gpio = 27;
int minidrawer = 1;
int side = 1;
unsigned int jtag_delay = 50;

static void sleep_ops(uint32_t the_ops)
{
    for (unsigned int i = 0; i < the_ops; i++)
        asm volatile("");
}

int gpioRead(int the_gpio)
{
    return (GET_GPIO(the_gpio)>>the_gpio);
}

void gpioWrite(int the_gpio, int the_value)
{
    if (the_value == 0) {
        GPIO_CLR = 1 << the_gpio;
    } else {
        GPIO_SET = 1 << the_gpio;
    }
}

void gpio_write(int the_tck_gpio, int the_tms_gpio, int the_tdi_gpio, int tck, int tms, int tdi)
{
    gpioWrite(the_tck_gpio, tck);
    gpioWrite(the_tms_gpio, tms);
    gpioWrite(the_tdi_gpio, tdi);
    for (unsigned int i = 0; i < jtag_delay; i++)
        asm volatile("");
}

uint32_t gpio_xfer(int n, uint32_t tms, uint32_t tdi)
{
    uint32_t tdo=0, tdo_a=0, tdo_b=0, tms_buffer=0, tdi_buffer=0;

    if (side == 0) {
        gpioWrite(side_gpio,0);
        tms_buffer=tms;
        tdi_buffer=tdi;
        for (int i = 0; i < n; i++) {
            gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, tms_buffer & 1, tdi_buffer & 1);
            tdo_a |= gpioRead(tdo_gpio_lut[minidrawer-1]) << i;
            gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 1, tms_buffer & 1, tdi_buffer & 1);
            tms_buffer >>= 1;
            tdi_buffer >>= 1;
        }

        gpioWrite(side_gpio,1);
        tms_buffer=tms;
        tdi_buffer=tdi;
        for (int i = 0; i < n; i++) {
            gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, tms_buffer & 1, tdi_buffer & 1);
            tdo_b |= gpioRead(tdo_gpio_lut[minidrawer-1]) << i;
            gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 1, tms_buffer & 1, tdi_buffer & 1);
            tms_buffer >>= 1;
            tdi_buffer >>= 1;
        }
        tdo = tdo_a;
    } else {
        for (int i = 0; i < n; i++) {
            gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, tms & 1, tdi & 1);
            tdo |= gpioRead(tdo_gpio_lut[minidrawer-1]) << i;
            gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 1, tms & 1, tdi & 1);
            tms >>= 1;
            tdi >>= 1;
        }
    }
    return tdo;
}

int gpio_read(int the_tdo_gpio)
{
    return gpioRead(the_tdo_gpio);
}

int gpio_init(void)
{
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0) {
        printf("can't open /dev/mem \n");
        exit(-1);
    }

    gpio_map = mmap(NULL, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, GPIO_BASE);
    close(mem_fd);

    if (gpio_map == MAP_FAILED) {
        printf("mmap error %d\n", (int)gpio_map);
        exit(-1);
    }

    gpio = (volatile unsigned *)gpio_map;

    INP_GPIO(side_gpio);
    OUT_GPIO(side_gpio);

    int i_from = (minidrawer == 0) ? 0 : minidrawer-1;
    int i_to   = (minidrawer == 0) ? 4 : minidrawer;

    for (int i = i_from; i < i_to; i++) {
        INP_GPIO(tdo_gpio_lut[i]);
        GPIO_PULL = 2;
        GPIO_PULLCLK0 = 1 << tdo_gpio_lut[i];
        sleep_ops(1000);
        GPIO_PULL = 0;
        GPIO_PULLCLK0 = 0;
        sleep_ops(1000);

        INP_GPIO(tdi_gpio_lut[i]);
        INP_GPIO(tck_gpio_lut[i]);
        INP_GPIO(tms_gpio_lut[i]);
        OUT_GPIO(tdi_gpio_lut[i]);
        OUT_GPIO(tck_gpio_lut[i]);
        OUT_GPIO(tms_gpio_lut[i]);

        INP_GPIO(md_enable_gpio_lut[i]);
        OUT_GPIO(md_enable_gpio_lut[i]);

        gpioWrite(tdi_gpio_lut[i], 0);
        gpioWrite(tck_gpio_lut[i], 0);
        gpioWrite(tms_gpio_lut[i], 1);
        gpioWrite(md_enable_gpio_lut[i], 1);
    }
    return ERROR_OK;
}
