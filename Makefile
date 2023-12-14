.DEFAULT_GOAL: all

TARGETS := all clean install

SUBDIRS := cpp doc

$(TARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TARGETS) $(SUBDIRS)
