CC              = gcc

CFLAGS          = -Wall -g -I/opt/local/include -Ihidapi -O
LDFLAGS         = -g
LIBS            = -L/opt/local/lib -lusb

# Mac OS X
LIBS            += -framework IOKit -framework CoreFoundation

PROG_OBJS       = pic32prog.o target.o executive.o hid.o \
                  adapter-pickit2.o adapter-boot.o adapter-mpsse.o

all:            pic32prog

load:           demo1986ve91.srec
		pic32prog $<

pic32prog:      $(PROG_OBJS)
		$(CC) $(LDFLAGS) -o $@ $(PROG_OBJS) $(LIBS)

adapter-mpsse:	adapter-mpsse.c
		$(CC) $(LDFLAGS) $(CFLAGS) -DSTANDALONE -o $@ adapter-mpsse.c $(LIBS)

pic32prog.po:	*.c
		xgettext --from-code=utf-8 --keyword=_ pic32prog.c target.c adapter-lpt.c -o $@

pic32prog-ru.mo: pic32prog-ru.po
		msgfmt -c -o $@ $<

pic32prog-ru-cp866.mo ru/LC_MESSAGES/pic32prog.mo: pic32prog-ru.po
		iconv -f utf-8 -t cp866 $< | sed 's/UTF-8/CP866/' | msgfmt -c -o $@ -
		cp pic32prog-ru-cp866.mo ru/LC_MESSAGES/pic32prog.mo

hid.o:          hidapi/hid-mac.c
		$(CC) $(CFLAGS) -c -o $@ $<

clean:
		rm -f *~ *.o core pic32prog adapter-mpsse pic32prog.po

install:	pic32prog #pic32prog-ru.mo
		install -c -s pic32prog /usr/local/bin/pic32prog
#		install -c -m 444 pic32prog-ru.mo /usr/local/share/locale/ru/LC_MESSAGES/pic32prog.mo
###
adapter-mpsse.o: adapter-mpsse.c adapter.h
adapter-pickit2.o: adapter-pickit2.c adapter.h pickit2.h pic32.h
executive.o: executive.c pic32.h
pic32prog.o: pic32prog.c target.h localize.h
target.o: target.c target.h adapter.h localize.h pic32.h
