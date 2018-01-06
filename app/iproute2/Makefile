
GLOBAL_LIB = ../lib
VPATH = ./

SOURCES = $(foreach dir, $(VPATH), $(wildcard $(dir)/*.c))
OBJS = $(SOURCES:%.c=%.o)

TOOL_PREFIX = $(GLOBAL_PREFIX)

COMPILE_TOOLS = $(TOOL_PREFIX)gcc
AR = $(TOOL_PREFIX)ar
RANLIB = $(TOOL_PREFIX)ranlib

CFLAGS = $(GLOBAL_FLAG) -DCONFIG_SUPPORT_NUI_GPL_PROC
CFLAGS += -I./include -I./include/net/iproute2
LIB = ./libiproute.a

all : $(LIB) 

$(LIB) : $(OBJS)
	$(AR) r $@ $(OBJS)
	$(RANLIB) $@
	cp $@ $(GLOBAL_LIB) 

%.o:%.c
	$(COMPILE_TOOLS) $(CFLAGS) -c $^ -o $@

clean :
	rm -f $(OBJS) *.bak $(LIB)

