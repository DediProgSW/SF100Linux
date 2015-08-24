#
# This file is part of the DediProg project.
#
#

PROGRAM = dpcmd
CC      = gcc
CFLAGS  = -Os -Wall -lpthread

FEATURE_LIBS += -lusb -lpthread
PROGRAMMER_OBJS += dpcmd.o usbdriver.o FlashCommand.o SerialFlash.o parse.o board.o project.o IntelHexFile.o MotorolaFile.o


$(PROGRAM): $(PROGRAMMER_OBJS)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(PROGRAMMER_OBJS) $(FEATURE_LIBS)


clean:
	rm -f $(PROGRAM) $(PROGRAM).exe *.o *.d *.c~ *.h~ *~

