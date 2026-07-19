# Native build (on the Pi):        make
# Cross build (on your host):      make CROSS_COMPILE=aarch64-linux-gnu- SYSROOT=$(HOME)/pi-sysroot
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -O2
LDLIBS := -lgpiod

ifdef SYSROOT
CFLAGS += --sysroot=$(SYSROOT)
LDFLAGS += --sysroot=$(SYSROOT)
endif

all: blink

blink: blink.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f blink

.PHONY: all clean
