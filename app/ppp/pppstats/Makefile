#
# pppstats makefile
# $Id: Makefile,v 1.1 2006/12/31 09:04:24 lzl Exp $
#

DESTDIR = /usr/local
BINDIR = $(DESTDIR)/sbin
MANDIR = $(DESTDIR)/share/man/man8

PPPSTATSRCS = pppstats.c
PPPSTATOBJS = pppstats.o

#CC = gcc
COMPILE_FLAGS = -I../include
LIBS =

INSTALL= install

CFLAGS = $(GLOBAL_FLAG) $(COMPILE_FLAGS)

all: pppstats

install: pppstats
	-mkdir -p $(MANDIR)
	$(INSTALL) -s -c pppstats $(BINDIR)
	$(INSTALL) -c -m 444 pppstats.8 $(MANDIR)

pppstats: $(PPPSTATSRCS)
	$(CC) $(CFLAGS) -o pppstats pppstats.c $(LIBS)

clean:
	rm -f pppstats *~ #* core

depend:
	cpp -M $(CFLAGS) $(PPPSTATSRCS) >.depend
#	makedepend $(CFLAGS) $(PPPSTATSRCS)
