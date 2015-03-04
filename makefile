CC              = gcc

SVNVERS         = $(shell git rev-parse --short HEAD)
UNAME           = $(shell uname)
CFLAGS          = -Wall -g -O -Ihidapi -DSVNVERSION='"$(SVNVERS)"'
LDFLAGS         = -g

# Linux
ifeq ($(UNAME),Linux)
    LIBS        += -Wl,-Bstatic -lusb-1.0 -Wl,-Bdynamic -lpthread -ludev
    HIDSRC      = hidapi/hid-libusb.c
endif

# Mac OS X
ifeq ($(UNAME),Darwin)
    CC          += -arch i386 -arch x86_64
    LIBS        += -framework IOKit -framework CoreFoundation
    HIDSRC      = hidapi/hid-mac.c
endif

PROG_OBJS       = pic32prog.o target.o executive.o hid.o \
                  adapter-pickit2.o adapter-hidboot.o adapter-an1388.o \
                  adapter-bitbang.o adapter-stk500v2.o adapter-uhb.o \
                  family-mx1.o family-mx3.o family-mz.o

# Olimex ARM-USB-Tiny JTAG adapter: requires libusb-0.1
CFLAGS          += -DUSE_MPSSE
PROG_OBJS       += adapter-mpsse.o
ifeq ($(UNAME),Linux)
    # Use 'sudo port install libusb-0.1-dev'
    LIBS        += -Wl,-Bstatic -lusb -Wl,-Bdynamic
endif
ifeq ($(UNAME),Darwin)
    # Use 'sudo port install libusb-legacy'
    CFLAGS      += -I/opt/local/include/libusb-legacy
    LIBS        += /opt/local/lib/libusb-legacy/libusb-legacy.a
endif

# Add support for UART bootloader (AN1388 serial protocol).
#CFLAGS          += -DUSE_AN1388_UART
#PROG_OBJS       += adapter-an1388-uart.o

all:            pic32prog

pic32prog:      $(PROG_OBJS)
		$(CC) $(LDFLAGS) -o $@ $(PROG_OBJS) $(LIBS)

hid.o:          $(HIDSRC)
		$(CC) $(CFLAGS) -c -o $@ $<

load:           demo1986ve91.srec
		pic32prog $<

adapter-mpsse:	adapter-mpsse.c
		$(CC) $(LDFLAGS) $(CFLAGS) -DSTANDALONE -o $@ adapter-mpsse.c $(LIBS)

pic32prog.po:	*.c
		xgettext --from-code=utf-8 --keyword=_ pic32prog.c target.c adapter-lpt.c -o $@

pic32prog-ru.mo: pic32prog-ru.po
		msgfmt -c -o $@ $<

pic32prog-ru-cp866.mo ru/LC_MESSAGES/pic32prog.mo: pic32prog-ru.po
		iconv -f utf-8 -t cp866 $< | sed 's/UTF-8/CP866/' | msgfmt -c -o $@ -
		cp pic32prog-ru-cp866.mo ru/LC_MESSAGES/pic32prog.mo

clean:
		rm -f *~ *.o core pic32prog adapter-mpsse pic32prog.po

install:	pic32prog #pic32prog-ru.mo
		install -c -s pic32prog /usr/local/bin/pic32prog
#		install -c -m 444 pic32prog-ru.mo /usr/local/share/locale/ru/LC_MESSAGES/pic32prog.mo
###
adapter-an1388.o: adapter-an1388.c adapter.h hidapi/hidapi.h pic32.h
adapter-hidboot.o: adapter-hidboot.c adapter.h hidapi/hidapi.h pic32.h
adapter-mpsse.o: adapter-mpsse.c adapter.h
adapter-pickit2.o: adapter-pickit2.c adapter.h pickit2.h pic32.h
executive.o: executive.c pic32.h
pic32prog.o: pic32prog.c target.h localize.h
target.o: target.c target.h adapter.h localize.h pic32.h
