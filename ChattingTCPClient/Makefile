EXECUTABLE=client
CXXSOURCES= ChattingTCPClient.cpp

CSOURCES=


#
# Compiler
#
#CROSS_COMPILE=arm-linux-gnueabihf-
CROSS_COMPILE=
AR=${CROSS_COMPILE}ar
AS=${CROSS_COMPILE}as
CC=${CROSS_COMPILE}gcc
CXX=${CROSS_COMPILE}g++
LD=${CROSS_COMPILE}ld
LDD=${CROSS_COMPILE}ldd
NM=${CROSS_COMPILE}nm
RANLIB=${CROSS_COMPILE}ranlib
READELF=${CROSS_COMPILE}readelf
OBJCOPY=${CROSS_COMPILE}objcopy
OBJDUMP=${CROSS_COMPILE}objdump
STRINGS=${CROSS_COMPILE}strings
STRIP=${CROSS_COMPILE}strip
SIZE=${CROSS_COMPILE}size


#
# Compiler Option
#
MODE            ?= debug        # debug / release
DEBUG           := -g
OPTIMIZE        := -O2
WARNING         := -Wall
MANUAL_LINKER   := -c
LIBCXXVER	:= -std=c++11
ifeq ($(MODE), release)
CFLAGS=$(MANUAL_LINKER) $(WARNING) $(OPTIMIZE) $(LIBCXXVER)
else
CFLAGS=$(MANUAL_LINKER) $(WARNING) $(OPTIMIZE) $(LIBCXXVER) $(DEBUG)
endif

#
# Linker Option
#
LIBPATH         =-L./lib
INCLUDEPATH     =-I./include
LIBS            =-lpthread -lboost_system -lboost_thread
LDFLAGS =  $(LIBPATH) $(INCLUDEPATH) $(LIBS)


CXXOBJECTS=$(CXXSOURCES:.cpp=.o)
COBJECTS=$(CSOURCES:.c=.o)

all: $(CXXSOURCES) $(CSOURCES) $(EXECUTABLE)


$(EXECUTABLE): $(CXXOBJECTS) $(COBJECTS)
	$(CXX)  $(CXXOBJECTS) $(COBJECTS) $(LDFLAGS) -o $@

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@
.c.o:
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(COBJECTS)
	rm -f $(CXXOBJECTS)

tags:
	ctags -R --c++-kinds=+p --fields=+iaS --extra=+q .

distclean: clean
	rm -f tags
	rm -f cscope.out
	rm -f cscope.files
	rm -f $(EXECUTABLE)
