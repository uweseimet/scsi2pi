##---------------------------------------------------------------------------
##
## SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
##
## Copyright (C) 2023-2026 Uwe Seimet
##
##---------------------------------------------------------------------------

.DEFAULT_GOAL := all

TARGETS := all fullspec standard tests test clean install

SUBDIRS := cpp doc

$(TARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C "$@" $(or $(MAKECMDGOALS),all)

.PHONY: $(TARGETS) $(SUBDIRS)
