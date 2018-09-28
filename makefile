CC              = gcc

GITCOUNT        = $(shell git rev-list HEAD --count)
UNAME           = $(shell uname)
CFLAGS          = -Wall -g -O -Ihidapi/hidapi -DGITCOUNT='"$(GITCOUNT)"'
LDFLAGS         = -g
CCARCH          =
LIBUSB_LIBS     = $(shell pkg-config libusb-1.0 --libs)

# Linux
ifeq ($(UNAME),Linux)
ifeq (,$(LIBUSB_LIBS))
    LIBS        += -Wl,-Bstatic -lusb-1.0 -Wl,-Bdynamic
else
    LIBS        += $(LIBUSB_LIBS)
endif
    LIBS        += -lpthread -ludev
    HIDLIB      = hidapi/libusb/.libs/libhidapi-libusb.a
endif

# Mac OS X
ifeq ($(UNAME),Darwin)
    LIBS        += -framework IOKit -framework CoreFoundation
    HIDLIB      = hidapi/mac/.libs/libhidapi.a
    UNIV_ARCHS  = $(shell grep '^universal_archs' /opt/local/etc/macports/macports.conf)
    ifneq ($(findstring i386,$(UNIV_ARCHS)),)
        CCARCH  += -arch i386
    endif
    ifneq ($(findstring x86_64,$(UNIV_ARCHS)),)
        CCARCH  += -arch x86_64
    endif
    CC          += $(CCARCH)
endif

PROG_OBJS       = pic32prog.o target.o executive.o serial.o \
                  adapter-pickit2.o adapter-hidboot.o adapter-an1388.o \
                  adapter-bitbang.o adapter-stk500v2.o adapter-uhb.o \
                  adapter-an1388-uart.o configure.o \
                  family-mx1.o family-mx3.o family-mz.o $(HIDLIB)

# JTAG adapters based on FT2232 chip
CFLAGS          += -DUSE_MPSSE
PROG_OBJS       += adapter-mpsse.o
ifeq ($(UNAME),Darwin)
    # Use 'sudo port install libusb'
    CFLAGS      += -I/opt/local/include
    LIBS        += /opt/local/lib/libusb-1.0.a -lobjc
endif

all:            pic32prog

pic32prog:      $(PROG_OBJS)
		$(CC) $(LDFLAGS) -o $@ $(PROG_OBJS) $(LIBS)

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
		rm -f *~ *.o core pic32prog adapter-mpsse pic32prog.po hidapi/ar-lib hidapi/compile
		if [ -f hidapi/Makefile ]; then make -C hidapi clean; fi

install:	pic32prog #pic32prog-ru.mo
		install -c -s pic32prog /usr/local/bin/pic32prog
#		install -c -m 444 pic32prog-ru.mo /usr/local/share/locale/ru/LC_MESSAGES/pic32prog.mo

hidapi/hidapi/hidapi.h:
		git submodule update --init

$(HIDLIB):      hidapi/hidapi/hidapi.h
		if [ ! -f hidapi/configure ]; then cd hidapi && ./bootstrap; fi
		cd hidapi && ./configure --enable-shared=no CFLAGS='$(CCARCH)'
		make -C hidapi

###
adapter-an1388-uart.o: adapter-an1388-uart.c adapter.h pic32.h serial.h
adapter-an1388.o: adapter-an1388.c adapter.h hidapi/hidapi/hidapi.h pic32.h
adapter-bitbang.o: adapter-bitbang.c adapter.h pic32.h serial.h \
  bitbang/ICSP_v1E.inc
adapter-hidboot.o: adapter-hidboot.c adapter.h hidapi/hidapi/hidapi.h pic32.h
adapter-mpsse.o: adapter-mpsse.c adapter.h pic32.h
adapter-pickit2.o: adapter-pickit2.c adapter.h hidapi/hidapi/hidapi.h pickit2.h \
  pic32.h
adapter-stk500v2.o: adapter-stk500v2.c adapter.h pic32.h serial.h
adapter-uhb.o: adapter-uhb.c adapter.h hidapi/hidapi/hidapi.h pic32.h
configure.o: configure.c target.h adapter.h
executive.o: executive.c pic32.h
family-mx1.o: family-mx1.c pic32.h
family-mx3.o: family-mx3.c pic32.h
family-mz.o: family-mz.c pic32.h
pic32prog.o: pic32prog.c target.h adapter.h serial.h localize.h
serial.o: serial.c adapter.h
target.o: target.c target.h adapter.h localize.h pic32.h
