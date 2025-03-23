##---------------------------------------------------------------------------
##
## SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
##
## Copyright (C) 2023-2025 Uwe Seimet
##
##---------------------------------------------------------------------------

.DEFAULT_GOAL: all

CONNECT_TYPE ?= FULLSPEC

TARGET := /opt/scsi2pi

INSTALL_DIR := $(TARGET)/man/man1/

MANPAGES = s2p.1 \
	s2pctl.1
ifeq ($(CONNECT_TYPE), FULLSPEC)
	MANPAGES += s2pdump.1 \
	s2pexec.1 \
	s2pproto.1
endif

INSTALL_PAGES := $(addprefix $(INSTALL_DIR),$(MANPAGES))

all: doc

test:

doc:

clean:

install: $(INSTALL_PAGES)

$(INSTALL_DIR)%.1: %.1 | $(INSTALL_DIR)
	cp $< $(INSTALL_DIR)

$(INSTALL_DIR):
	mkdir -p $@
