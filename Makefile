#
# This file is part of the DediProg project.
#
#

PROGRAM = dpcmd
CC      = gcc
PREFIX ?= /usr/local

PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -Wall -std=gnu99
CFLAGS += $(shell $(PKG_CONFIG) --cflags libusb-1.0)

LDFLAGS ?=
LDFLAGS += -lpthread
LDFLAGS += $(shell $(PKG_CONFIG) --libs libusb-1.0)

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

SRCS = dpcmd.c usbdriver.c FlashCommand.c SerialFlash.c parse.c board.c project.c IntelHexFile.c MotorolaFile.c

PROGRAMMER_OBJS := $(SRCS:%.c=%.o)

$(PROGRAM): $(PROGRAMMER_OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c
%.o : %.c $(DEPDIR)/%.d | $(DEPDIR)
	$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(SRCS:%.c=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))

clean:
	rm -vf $(PROGRAM) $(PROGRAM).exe *.o
	rm -rvf $(DEPDIR)

install: $(PROGRAM)
	@[ $(shell id -u) -eq 0 ] || (echo "Error: install needs root privileges" && false)
	@mkdir -vp $(PREFIX)/bin $(PREFIX)/share/DediProg
	@echo -n "install: " && install -v -o 0 -g 0 -m 0755 $(PROGRAM) $(PREFIX)/bin/$(PROGRAM)
	strip $(PREFIX)/bin/$(PROGRAM)
	@echo -n "install: " && install -v -o 0 -g 0 -m 0644 ChipInfoDb.dedicfg $(PREFIX)/share/DediProg/ChipInfoDb.dedicfg
	@echo -n "install: " && install -v -o 0 -g 0 -m 0644 60-dediprog.rules /etc/udev/rules.d/60-dediprog.rules

uninstall:
	@[ $(shell id -u) -eq 0 ] || (echo "Error: uninstall needs root privileges" && false)
	@rm -vf $(PREFIX)/bin/$(PROGRAM) $(PREFIX)/share/DediProg/ChipInfoDb.dedicfg /etc/udev/rules.d/60-dediprog.rules
	@[ -d "$(PREFIX)/share/DediProg" ] && rmdir -v $(PREFIX)/share/DediProg || true
