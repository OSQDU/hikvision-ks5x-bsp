#	$Id: Makefile,v 1.2 2007/01/20 03:23:50 lzl Exp $


DESTDIR = /usr/local
BINDIR = $(DESTDIR)/sbin
MANDIR = $(DESTDIR)/share/man/man8

CDEF1=	-DTERMIOS			# Use the termios structure
CDEF2=	-DSIGTYPE=void			# Standard definition
CDEF3=	-UNO_SLEEP			# Use the usleep function
CDEF4=	-DFNDELAY=O_NDELAY		# Old name value
CDEFS=	$(CDEF1) $(CDEF2) $(CDEF3) $(CDEF4)

CFLAGS=	$(GLOBAL_FLAG) $(CDEFS)

CC = $(GLOBAL_PREFIX)gcc
STRIP = $(GLOBAL_PREFIX)strip
INSTALL= install

all:	chat

chat:	chat.o

####gcov Test,Add by zhubin 20130515 ######
ifeq ($(support_gcov), y)
	$(CC) -o chat chat.o $(LDFLAGS) 
else
	$(CC) -o chat chat.o
endif	
	$(STRIP) $@
	$(STRIP) -x -R .note -R .comment $@

chat.o:	chat.c
	$(CC) -c $(CFLAGS) -o chat.o chat.c

install: chat
	mkdir -p $(BINDIR)
	$(INSTALL) -s -c chat $(BINDIR)
	$(INSTALL) -c -m 644 chat.8 $(MANDIR)

clean:
	rm -f chat.o chat *~
