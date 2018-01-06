# Generated automatically from Makefile.in by configure.
# $Id: Makefile,v 1.1 2006/12/31 09:04:41 lzl Exp $
#
# Makefile for event-handling library
#
# Copyright 2002 Roaring Penguin Software Inc.
#
# This software may be distributed according to the terms of the GNU
# General Public License, version 2 or (at your option) any later version.
# LIC: GPL

OBJS=event.o event_tcp.o hash.o event_sig.o
SRCS=$(OBJS:.o=.c)
HDRS=event.h event_tcp.h eventpriv.h hash.h

CFLAGS=-g -O2 -Wall -Wstrict-prototypes -ansi -I..

#CROSS_COMPILE = arm-linux-
CROSS_COMPILE=arm-wrs-linux-gnueabi-armv5tel-uclibc_small-
CC = $(CROSS_COMPILE)gcc 
AR = $(CROSS_COMPILE)ar
RANLIB = $(TOOL_PREFIX)ranlib

all: libevent.a

libevent.a: $(OBJS)
	rm -f libevent.a
	$(AR) -cq libevent.a $(OBJS)
	$(RANLIB) libevent.a

event.o: event.c $(HDRS)
	$(CC) $(CFLAGS) -c -o event.o event.c

hash.o: hash.c $(HDRS)
	$(CC) $(CFLAGS) -c -o hash.o hash.c

event_sig.o: event_sig.c $(HDRS)
	$(CC) $(CFLAGS) -c -o event_sig.o event_sig.c

event_tcp.o: event_tcp.c $(HDRS)
	$(CC) $(CFLAGS) -c -o event_tcp.o event_tcp.c

clean: FORCE
	rm -f *.a *.o *~

FORCE:

.phony: FORCE
