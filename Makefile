##---------------------------------------------------------------------------
##
## SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
##
## Copyright (C) 2023-2025 Uwe Seimet
##
##---------------------------------------------------------------------------

.DEFAULT_GOAL: all

TARGETS := all test clean install

SUBDIRS := cpp doc

$(TARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TARGETS) $(SUBDIRS)
