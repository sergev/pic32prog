###########################################
# Simple Makefile for HIDAPI test program
#
# Alan Ott
# Signal 11 Software
# 2010-06-01
###########################################

all: libs

libs: libhidapi-libusb.so libhidapi-libusb.a

CC       ?= gcc
CFLAGS   ?= -Wall -g -fpic

CXX      ?= g++
CXXFLAGS ?= -Wall -g -fpic

LDFLAGS  ?= -Wall -g

COBJS_LIBUSB = hid.o
COBJS = $(COBJS_LIBUSB)
OBJS      = $(COBJS) $(CPPOBJS)
LIBS_USB  = `pkg-config libusb-1.0 --libs` -lrt -lpthread
LIBS      = $(LIBS_USB)
INCLUDES ?= -I../hidapi `pkg-config libusb-1.0 --cflags`


# Shared Libs
libhidapi-libusb.so: $(COBJS_LIBUSB)
	$(CC) $(LDFLAGS) $(LIBS_USB) -shared -fpic -Wl,-soname,$@.0 $^ -o $@

libhidapi-libusb.a: $(COBJS_LIBUSB)
	$(AR) rcs $@ $<

# Objects
$(COBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS) libhidapi-libusb.so 

.PHONY: clean libs
