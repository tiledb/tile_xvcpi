# Makefile for XVC Server

CC = gcc
CFLAGS = -O2 -Wall -pthread

# Sources
SRCS = main.c gpio_io.c tcp_server.c xvc_protocol.c
OBJS = $(SRCS:.c=.o)

# Output executable
TARGET = tile_xvcpi

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
