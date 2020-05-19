#
# This file is part of the DediProg project.
#
#

PROGRAM = dpcmd
FEATURE_LIBS += -lusb -lpthread
CFLAGS  = -O2 -Wall
LDFLAGS = -z muldefs $(FEATURE_LIBS)

UNAME_OS := $(shell lsb_release -si)

ifneq ($(UNAME_OS),Ubuntu)
     CFLAGS+=-D_NON_UBUNTU
endif

PROGRAMMER_OBJS += dpcmd.o usbdriver.o FlashCommand.o SerialFlash.o parse.o board.o project.o IntelHexFile.o MotorolaFile.o

$(PROGRAM): $(PROGRAMMER_OBJS)
	$(CC) $(LDFLAGS) $(PROGRAMMER_OBJS) -o $@

clean:
	rm -f $(PROGRAM) $(PROGRAM).exe *.o *.d *.c~ *.h~ *~

