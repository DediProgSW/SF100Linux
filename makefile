#
# This file is part of the DediProg project.
#
#

PROGRAM = dpcmd
CC      = gcc
CFLAGS := -Os -lpthread -Wall -Wunused-parameter

UNAME_OS := $(shell lsb_release -si)

ifneq ($(UNAME_OS),Ubuntu)
     CFLAGS+=-D_NON_UBUNTU
endif

FEATURE_LIBS += -lusb -lpthread
PROGRAMMER_OBJS += dpcmd.o usbdriver.o FlashCommand.o SerialFlash.o parse.o board.o project.o IntelHexFile.o MotorolaFile.o


$(PROGRAM): $(PROGRAMMER_OBJS)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(PROGRAMMER_OBJS) $(FEATURE_LIBS)


clean:
	rm -f $(PROGRAM) $(PROGRAM).exe *.o *.d *.c~ *.h~ *~

