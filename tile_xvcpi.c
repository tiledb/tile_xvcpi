/*
 * Description: Xilinx Virtual Cable Server for Raspberry Pi
 *
 * See Licensing information at End of File.
 */

#define BCM2708_PERI_BASE        0x3F000000
#define BCM2711_PERI_BASE        0xFE000000
#define GPIO_BASE                (BCM2711_PERI_BASE + 0x200000) /* GPIO controller */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
void *gpio_map;

// I/O access
volatile unsigned *gpio;
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock


#define ERROR_JTAG_INIT_FAILED -1
#define ERROR_OK 1


static uint32_t gpio_xfer(int n, uint32_t tms, uint32_t tdi);
static int gpio_read(int the_tdo_gpio);
static void gpioWrite(int the_gpio, int the_value);
static int gpioRead(int the_gpio);
static void gpio_write(int the_tck_gpio, int the_tms_gpio, int the_tdi_gpio, int tck, int tms, int tdi);

static int gpio_init(void);

// pinout and lut
static int tck_gpio_lut[4] = {4,11,25,6}; 
static int tms_gpio_lut[4] = {18,22,16,13}; 
static int tdi_gpio_lut[4] = {2,10,23,0}; 
static int tdo_gpio_lut[4] = {3,9,24,5};
static int md_enable_gpio_lut[4] = {17,7,26,1};

static int side_gpio = 27;

static int tcp_port = 2542;
static int minidrawer = 1;
static int side = 1;
//static int gpio_initialized = 0;

/* Transition delay coefficients */
static unsigned int jtag_delay = 50; //50; //50;

static void sleep_ops(uint32_t the_ops)
{
    for (unsigned int i = 0; i < the_ops; i++)
    asm volatile("");
  };

static uint32_t gpio_xfer(int n, uint32_t tms, uint32_t tdi) {
  uint32_t tdo=0, tdo_a = 0, tdo_b = 0, tms_buffer = 0, tdi_buffer = 0;

  if (side == 0) {
    //fprintf(stderr, "side a");
    gpioWrite(side_gpio,0);
    tms_buffer=tms;
    tdi_buffer=tdi;
    for (int i = 0; i < n; i++) {      
      gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, tms_buffer & 1, tdi_buffer & 1);
      tdo_a |= gpio_read(tdo_gpio_lut[minidrawer-1]) << i;
      gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 1, tms_buffer & 1, tdi_buffer & 1);
      tms_buffer >>= 1;
      tdi_buffer >>= 1;
    }
    //fprintf(stderr, "side b");
    gpioWrite(side_gpio,1);
    tms_buffer=tms;
    tdi_buffer=tdi; //tdo_a;
    for (int i = 0; i < n; i++) {
      gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, tms_buffer & 1, tdi_buffer & 1);
      tdo_b |= gpio_read(tdo_gpio_lut[minidrawer-1]) << i;
      gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 1, tms_buffer & 1, tdi_buffer & 1);
      tms_buffer >>= 1;
      tdi_buffer >>= 1;
    }
    
    tdo=tdo_a;
  }
  else {
    for (int i = 0; i < n; i++) {
      gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, tms & 1, tdi & 1);
      tdo |= gpio_read(tdo_gpio_lut[minidrawer-1]) << i;
      gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 1, tms & 1, tdi & 1);
      tms >>= 1;
      tdi >>= 1;    
    }
  }
  return tdo;
}

static int gpio_read(int the_tdo_gpio) { return gpioRead(the_tdo_gpio); }

static int gpioRead(int the_gpio)
{
  int g = the_gpio;
  //fprintf(stderr,"TDO: %d",GET_GPIO(g)>>the_gpio);
  return (GET_GPIO(g)>>the_gpio);
  }
static void gpioWrite(int the_gpio, int the_value)
  {
    if (the_value == 0) {
      GPIO_CLR = 1 << the_gpio;
    }
    else {
      GPIO_SET = 1 << the_gpio;
      }
    
    }
;

static void gpio_write(int the_tck_gpio, int the_tms_gpio, int the_tdi_gpio, int tck, int tms, int tdi) {
  gpioWrite(the_tck_gpio, tck);
  gpioWrite(the_tms_gpio, tms);
  gpioWrite(the_tdi_gpio, tdi);

  for (unsigned int i = 0; i < jtag_delay; i++)
    asm volatile("");
}


static int gpio_init(void) {
  
     /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }

   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );

   close(mem_fd); //No need to keep mem_fd open after mmap

   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;

  INP_GPIO(side_gpio);
  OUT_GPIO(side_gpio);
  int i_from = 0, i_to = 4;
  if (minidrawer == 0) {
    i_from = 0;
    i_to = 4;
    }
  else {
    i_from = minidrawer-1;
    i_to = minidrawer;
    }
  fprintf(stderr, "Initializing IOs from Minidrawer: %d, to Minidrawer: %d \n", i_from, i_to);
  /*
  for (int i = 0; i<4; i++){
    INP_GPIO(tdo_gpio_lut[i]);
    GPIO_PULL = 2;
    GPIO_PULLCLK0 = 1<<tdo_gpio_lut[i];
    sleep_ops(1000);
    GPIO_PULL = 0;
    GPIO_PULLCLK0 = 0;
    sleep_ops(1000);
    }
  */
  for (int i = i_from; i < i_to; i++) {
    fprintf(stderr, "Initializing IOs for Minidrawer: %d \n", i+1);
    
    INP_GPIO(tdo_gpio_lut[i]);
    GPIO_PULL = 2;
    GPIO_PULLCLK0 = 1<<tdo_gpio_lut[i];
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
    gpioWrite(md_enable_gpio_lut[i],1);
  }
  return ERROR_OK;
}

static int verbose = 0;

static int sread(int fd, void *target, int len) {
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
      if (sread(fd, cmd, 6) != 1)
        return 1;
      memcpy(result, xvcInfo, strlen(xvcInfo));
      if (write(fd, result, strlen(xvcInfo)) != strlen(xvcInfo)) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'getinfo'\n", (int)time(NULL));
        printf("\t Replied with %s\n", xvcInfo);
      }
      break;
    } else if (memcmp(cmd, "se", 2) == 0) {
      if (sread(fd, cmd, 9) != 1)
        return 1;
      memcpy(result, cmd + 5, 4);
      if (write(fd, result, 4) != 4) {
        perror("write");
        return 1;
      }
      if (verbose) {
        printf("%u : Received command: 'settck'\n", (int)time(NULL));
        printf("\t Replied with '%.*s'\n\n", 4, cmd + 5);
      }
      break;
    } else if (memcmp(cmd, "sh", 2) == 0) {
      if (sread(fd, cmd, 4) != 1)
        return 1;
      if (verbose) {
        printf("%u : Received command: 'shift'\n", (int)time(NULL));
      }
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

    gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, 1, 1);

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

    gpio_write(tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], 0, 1, 0);

    if (write(fd, result, nr_bytes) != nr_bytes) {
      perror("write");
      return 1;
    }

  } while (1);
  /* Note: Need to fix JTAG state updates, until then no exit is allowed */
  return 0;
}

int main(int argc, char **argv) {
  int i;
  int s;
  int c;
  char side_char = 'a';
  int p=-1;
  struct sockaddr_in address;

  opterr = 0;


  while ((c = getopt(argc, argv, "?vj:p:m:s:")) != -1)
    switch (c) {
      case 'v':
        verbose = 1;
        //fprintf(stderr, "verbose on\n");
        break;
      case 'p':
        p=atoi(optarg);
        fprintf(stderr, "TCP port parameter: %d\n", p);
        break;
      case 'j':
        jtag_delay = atoi(optarg);
        fprintf(stderr, "Setting jtag_delay: %d\n", jtag_delay);
        //fprintf(stderr, "verbose on\n");
        break;
      case 'm':
        minidrawer = atoi(optarg);
        if (p==0) {
          tcp_port=2542;
        }
        else if (p<0) {
          tcp_port=2542-1+minidrawer;
        }
        else {
          tcp_port=p;
        }
        fprintf(stderr, "Setting tcp_port from parameter: %d\n", tcp_port);
        fprintf(stderr, "Selecting minidrawer: %d\n", minidrawer);
        break;
      case 's':
        //fprintf (stderr, "(side: %d) \n", side);
        side = atoi(optarg);
        break;
      case '?':
        fprintf(stderr, "help\n");
        fprintf(stderr, "usage: %s [-v] [-p select tcp port] [-m minidrawer from 1 to 4] [-s side 1=a and 2=b] [-j jtag delay for changing the speed of the gpio jtag operations]\n", *argv);
        return 1;
    }

  if (gpio_init() < 1) {
    fprintf(stderr, "Failed in gpio_init()\n");
    return -1;
  }
  gpioWrite(md_enable_gpio_lut[minidrawer-1],0);

  switch (side) {
    case 0:
      fprintf (stderr, "Broadcasting! (side: %d) \n", side);
      side_char='*';
      gpioWrite(side_gpio,0);
      break;
    case 1:
      fprintf (stderr, "Selecting side: %d (a) \n", side);
      side_char='a';
      gpioWrite(side_gpio,0);
      break;
    case 2:
      fprintf (stderr, "Selecting side: %d (b) \n", side);
      side_char='b';
      gpioWrite(side_gpio,1);
      break;
    }

    

  fprintf(stderr, "XVCPI initialized in tcp_port: %d,at speed delay factor: %d, with pins: \n",tcp_port , jtag_delay);
  fprintf(stderr, "Minidrawer: %d -> Side %c: \n", minidrawer, side_char);
  
  fprintf(stderr, "tcp_port:[%d]  tck:gpio[%d], tms:gpio[%d], tdi:gpio[%d], tdo:gpio[%d]\n", tcp_port, tck_gpio_lut[minidrawer-1], tms_gpio_lut[minidrawer-1], tdi_gpio_lut[minidrawer-1], tdo_gpio_lut[minidrawer-1]);

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    perror("socket");
    return 1;
  }
  i = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof i);
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(tcp_port);
  address.sin_family = AF_INET;

  if (bind(s, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind");
    return 1;
  }

  if (listen(s, 0) < 0) {
    perror("listen");
    return 1;
  }

  fd_set conn;
  int maxfd = 0;

  FD_ZERO(&conn);
  FD_SET(s, &conn);

  maxfd = s;

  while (1) {
    fd_set read = conn, except = conn;
    int fd;

    if (select(maxfd + 1, &read, 0, &except, 0) < 0) {
      perror("select");
      break;
    }

    for (fd = 0; fd <= maxfd; ++fd) {
      if (FD_ISSET(fd, &read)) {
        if (fd == s) {
          int newfd;
          socklen_t nsize = sizeof(address);

          newfd = accept(s, (struct sockaddr *)&address, &nsize);
          if (verbose)
            printf("connection accepted - fd %d\n", newfd);
          if (newfd < 0) {
            perror("accept");
          } else {
            int flag = 1;
            int optResult = setsockopt(newfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
            if (optResult < 0)
              perror("TCP_NODELAY error");
            if (newfd > maxfd) {
              maxfd = newfd;
            }
            FD_SET(newfd, &conn);
          }
        } else if (handle_data(fd)) {
          if (verbose)
            printf("connection closed - fd %d\n", fd);
          close(fd);
          FD_CLR(fd, &conn);
        }
      } else if (FD_ISSET(fd, &except)) {
        if (verbose)
          printf("connection aborted - fd %d\n", fd);
        close(fd);
        FD_CLR(fd, &conn);
        if (fd == s)
          break;
      }
    }
  }

  return 0;
}

/*
 * This work, "xvcpi.c", is a derivative of "xvcServer.c" (https://github.com/Xilinx/XilinxVirtualCable)
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 * "xvcServer.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
 * by Avnet and is used by Xilinx for XAPP1251.
 *
 * "xvcServer.c", is a derivative of "xvcd.c" (https://github.com/tmbinc/xvcd)
 * by tmbinc, used under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/).
 *
 * Portions of "xvcpi.c" are derived from OpenOCD (http://openocd.org)
 *
 * "xvcpi.c" is licensed under CC0 1.0 Universal (http://creativecommons.org/publicdomain/zero/1.0/)
 * by Derek Mulcahy.*
 */
