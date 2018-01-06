#!/bin/bash

GLOBAL_LIB = ../lib
VPATH = ./src
SOURCES = $(foreach dir, $(VPATH), $(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c, %.o, $(SOURCES))
DEPS = $(OBJS:.o=.d)

CROSS_COMPILE = $(GLOBAL_PREFIX)

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
STRIP = $(CROSS_COMPILE)strip  

CFLAG	= -DCONFIG_SUPPORT_NUI_GPL_PROC
INCLUDES=
CFLAGS	= $(CFLAG) $(GLOBAL_FLAG) $(OTHERS) $(INCLUDES)
CFLAGS += $(GLOBAL_FLAG) 
CPPFLAGS = -I./include


LDFLAGS = -L$(GLOBAL_LIB) -lpthread  -liproute -liptables -lrt


TARGET = netOpenProc
     
%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$

$(TARGET): $(OBJS) FORCE
	$(CC) $(OBJS) $(LDFLAGS) -o $@
#	$(STRIP) $@
#	$(STRIP) -x -R .note -R .comment $@
#	cp $@ $(G_DIR_PACK) -f
#	cp $@ $(G_DIR_NFS) -f

include $(DEPS)

.PHONY: all clean
PHONY += FORCE
FORCE:
	
all: $(TARGET)

clean:
	-rm -f $(TARGET) $(OBJS) $(DEPS)

