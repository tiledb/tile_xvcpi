# Compiler & flags
CC = gcc
CFLAGS = -O2 -Wall -pthread

# ---------------------------
# Executables
# ---------------------------
TARGET_XVCPI        = tile_xvcpi
TARGET_SCAN         = tile_jtag_scan
TARGET_CONTROL_XCKU = control_xcku035

# ---------------------------
# Source files
# ---------------------------

# tile_xvcpi server
SRCS_XVCPI = tile_xvcpi.c gpio_io.c tcp_server.c xvc_protocol.c
OBJS_XVCPI = $(SRCS_XVCPI:.c=.o)

# tile_jtag_scan CLI
SRCS_SCAN = tile_jtag_scan.c gpio_io.c
OBJS_SCAN = $(SRCS_SCAN:.c=.o)

# control_xcku035 CLI
SRCS_CONTROL_XCKU = control_xcku035.c gpio_io.c
OBJS_CONTROL_XCKU = $(SRCS_CONTROL_XCKU:.c=.o)

# ---------------------------
# Default target: build everything
# ---------------------------
all: $(TARGET_XVCPI) $(TARGET_SCAN) $(TARGET_CONTROL_XCKU)

# ---------------------------
# Build tile_xvcpi
# ---------------------------
$(TARGET_XVCPI): $(OBJS_XVCPI)
	$(CC) $(CFLAGS) -o $@ $^

# ---------------------------
# Build tile_jtag_scan
# ---------------------------
$(TARGET_SCAN): $(OBJS_SCAN)
	$(CC) $(CFLAGS) -o $@ $^

# ---------------------------
# Build control_xcku035
# ---------------------------
$(TARGET_CONTROL_XCKU): $(OBJS_CONTROL_XCKU)
	$(CC) $(CFLAGS) -o $@ $^

# ---------------------------
# Pattern rule for object files
# ---------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------
# Clean
# ---------------------------
clean:
	rm -f *.o $(TARGET_XVCPI) $(TARGET_SCAN) $(TARGET_CONTROL_XCKU)

.PHONY: all clean
