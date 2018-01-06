#!/bin/bash

GLOBAL_LIB = ../lib
VPATH = iptables libxtables extensions

SOURCES := $(foreach dir,$(VPATH),$(wildcard $(dir)/*))
C_SRCS   = $(filter %.c,$(SOURCES))
CPP_SRCS = $(filter %.cpp,$(SOURCES))
C_SRCS += libiptc/libip4tc.c libiptc/libip6tc.c 
SRCS = $(C_SRCS) $(CPP_SRCS)

C_OBJS   = $(C_SRCS:%.c=%.o)
CPP_OBJS = $(CPP_SRCS:%.cpp=%.o)
OBJS = $(C_OBJS) $(CPP_OBJS)
DEPS = $(OBJS:.o=.d)

PREFIX = $(GLOBAL_PREFIX)
CC = $(PREFIX)gcc
CXX = $(PREFIX)g++
STRIP = $(PREFIX)strip  
RANLIB = $(PREFIX)ranlib

CFLAGS = $(GLOBAL_FLAG) -DCONFIG_SUPPORT_NUI_GPL_PROC
CXXFLAGS = $(CFLAGS)

CPPFLAGS = -I. -Iinclude -Iiptables
LDFLAGS = -lm
CFLAGS += -DENABLE_IPV4 -DENABLE_IPV6 -DNO_SHARED_LIBS


TARGET = libiptables.a
     
%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CPPFLAGS) $< > $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$

%.d: %.cpp
	@set -e; rm -f $@; \
	 $(CXX) -MM $(CPPFLAGS) $< >  $@.$$$$; \
     sed 's,\($(notdir $*)\)\.o[ :]*,$*\.o $@ : ,g' < $@.$$$$ > $@; \
     rm -f $@.$$$$


$(TARGET): $(OBJS)
	$(AR) r $@ $(OBJS)
	$(RANLIB) $@
	cp $@ $(GLOBAL_LIB)

sinclude $(DEPS)

.PHONY: all clean

all: $(TARGET)

clean:
	-rm -f $(TARGET) $(OBJS) $(DEPS)
	

