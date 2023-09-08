#
# This file is part of the DediProg project.
#
#

# Make is silent per default, but 'make V=1' will show all compiler calls.
Q:=@
ifneq ($(V),1)
ifneq ($(Q),)
.SILENT:
endif
endif

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

all: $(PROGRAM)
	@printf "All done.\n"

$(PROGRAM): $(PROGRAMMER_OBJS)
	@printf "  LD  $@\n"
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

%.o : %.c
%.o : %.c $(DEPDIR)/%.d | $(DEPDIR)
	@printf "  CC  $@\n"
	$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(SRCS:%.c=$(DEPDIR)/%.d)
$(DEPFILES):

include $(wildcard $(DEPFILES))

clean:
	rm -vf $(PROGRAM) $(PROGRAM).exe *.o
	rm -rvf $(DEPDIR)

install: $(PROGRAM)
	[ $(shell id -u) -eq 0 ] || (echo "Error: install needs root privileges" && false)
	install -v -o 0 -g 0 -m 755 -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/share/DediProg
	echo -n "install: " && install -v -o 0 -g 0 -m 0755 $(PROGRAM) $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	strip $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	install -v -o 0 -g 0 -m 755 -d $(DESTDIR)$(PREFIX)/share/DediProg
	echo -n "install: " && install -v -o 0 -g 0 -m 0644 ChipInfoDb.dedicfg $(DESTDIR)$(PREFIX)/share/DediProg/ChipInfoDb.dedicfg
	install -v -o 0 -g 0 -m 755 -d $(DESTDIR)/etc/udev/rules.d
	echo -n "install: " && install -v -o 0 -g 0 -m 0644 60-dediprog.rules $(DESTDIR)/etc/udev/rules.d/60-dediprog.rules

uninstall:
	[ $(shell id -u) -eq 0 ] || (echo "Error: uninstall needs root privileges" && false)
	rm -vf $(DESTDIR)$(PREFIX)/bin/$(PROGRAM) $(DESTDIR)$(PREFIX)/share/DediProg/ChipInfoDb.dedicfg $(DESTDIR)/etc/udev/rules.d/60-dediprog.rules
	[ -d "$(DESTDIR)$(PREFIX)/share/DediProg" ] && rmdir -vd $(DESTDIR)$(PREFIX)/share/DediProg || true

# Run before every release / check-in to keep the source code tidy
release:
	chmod 644 *.c *.h README.md Makefile LICENSE .gitignore ChipInfoDb.dedicfg 60-dediprog.rules
	clang-format -i -style=WebKit *.c *.h
