#
# This file is part of the DediProg project.
#
#

PROGRAM = dpcmd
CC      = gcc
CFLAGS  = -O2 -Wall -lpthread -std=gnu99
PREFIX ?= /usr/local

UNAME_OS := $(shell which lsb_release 2>/dev/null && lsb_release -si)

ifneq ($(UNAME_OS),Ubuntu)
     CFLAGS+=-D_NON_UBUNTU
endif

FEATURE_LIBS += -lusb -lpthread
PROGRAMMER_OBJS += dpcmd.o usbdriver.o FlashCommand.o SerialFlash.o parse.o board.o project.o IntelHexFile.o MotorolaFile.o

$(PROGRAM): $(PROGRAMMER_OBJS) *.h Makefile
	$(CC) $(CFLAGS) -o $(PROGRAM) $(PROGRAMMER_OBJS) $(FEATURE_LIBS)

clean:
	@rm -vf $(PROGRAM) $(PROGRAM).exe *.o *.d

install: $(PROGRAM)
	@[ $(shell id -u) -eq 0 ] || (echo "Error: install needs root privileges" && false)
	@mkdir -vp $(PREFIX)/bin $(PREFIX)/share/DediProg
	@echo -n "install: " && install -v -o 0 -g 0 -m 0755 $(PROGRAM) $(PREFIX)/bin/$(PROGRAM)
	@strip $(PREFIX)/bin/$(PROGRAM)
	@echo -n "install: " && install -v -o 0 -g 0 -m 0644 ChipInfoDb.dedicfg $(PREFIX)/share/DediProg/ChipInfoDb.dedicfg
	@echo -n "install: " && install -v -o 0 -g 0 -m 0644 60-dediprog.rules /etc/udev/rules.d/60-dediprog.rules

uninstall:
	@[ $(shell id -u) -eq 0 ] || (echo "Error: uninstall needs root privileges" && false)
	@rm -vf $(PREFIX)/bin/$(PROGRAM) $(PREFIX)/share/DediProg/ChipInfoDb.dedicfg /etc/udev/rules.d/60-dediprog.rules
	@[ -d "$(PREFIX)/share/DediProg" ] && rmdir -v $(PREFIX)/share/DediProg || true
