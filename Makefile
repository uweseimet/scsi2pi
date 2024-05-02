.DEFAULT_GOAL: all

TARGETS := all test clean install

SUBDIRS := cpp doc

$(TARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TARGETS) $(SUBDIRS)
