.DEFAULT_GOAL: all

## Optional build flags:
##   CROSS_COMPILE= Specifies the compiler toolchain, e.g.
##                  arm-linux-gnueabihf-
CROSS_COMPILE =

CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)gcc-ar
STRIP = $(CROSS_COMPILE)strip

##   DEBUG=1       A Debug build includes the debugger symbols and
##                 disables compiler optimization. This is usually
##                 only by developers.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	# Debug compiler flags
	CXXFLAGS += -O0 -g -DDEBUG
else
	# Release compiler flags
	CXXFLAGS += -O3 -Wall -DNDEBUG
endif

CXXFLAGS += -std=c++20 -iquote . -D_FILE_OFFSET_BITS=64 -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -MD -MP -Wall -Wextra

ifeq ("$(shell uname -s)", "Linux")
	# -Wno-psabi might not work on non-Linux platforms
	CXXFLAGS += -Wno-psabi
endif

ifeq ("$(shell uname -s)", "FreeBSD")
	AR = $(CROSS_COMPILE)ar
	CXXFLAGS += -fexperimental-library -I/usr/local/include -Wno-unused-parameter
endif

ifeq ("$(shell uname -s)", "NetBSD")
	AR = $(CROSS_COMPILE)ar
	CXXFLAGS += -I/usr/pkg/include -Wno-unused-parameter -Wno-macro-redefined
endif

## EXTRA_FLAGS Can be used to pass special purpose flag
CXXFLAGS += $(EXTRA_FLAGS)

## Installation path
##   The path to install binaries and manpages in, default is /opt/scsi2pi
TARGET := /opt/scsi2pi

##   BOARD=FULLSPEC
##     Specifies the board type that you are using. Typically this is 
##     STANDARD or FULLSPEC. The default is FULLSPEC.
BOARD ?= FULLSPEC

CXXFLAGS += -DBOARD_$(BOARD)

##   ENABLE_*=1
##   DISABLE_*=1
##     Specifies the device types to build or not to build, default is to build all. Supported types:
##       SCHD SCSI Removable and Non-removable Hard Drive
##       SCCD SCSI CD-Rom Drive
##       SCMO SCSI Optical Memory
##       SCDP DaynaPort Network Adapter
##       SCLP SCSI Printer
##       SCHS Host Services
##       SAHD SASI Hard Drive
ifeq ($(or $(ENABLE_SCHD),$(ENABLE_SCMO),$(ENABLE_SCCD),$(ENABLE_SCDP),$(ENABLE_SCLP),$(ENABLE_SCHS),$(ENABLE_SAHD)),)
	ENABLE_SCHD = 1
	ENABLE_SCMO = 1
	ENABLE_SCCD = 1
	ENABLE_SCDP = 1
	ENABLE_SCLP = 1
	ENABLE_SCHS = 1
	ENABLE_SAHD = 1
endif

ifdef ENABLE_SCRM
	ENABLE_SCHD = 1
endif

ifdef DISABLE_SCHD
	ENABLE_SCHD =
endif
ifdef DISABLE_SCMO
	ENABLE_SCMO =
endif
ifdef DISABLE_SCCD
	ENABLE_SCCD =
endif
ifdef DISABLE_SCDP
	ENABLE_SCDP =
endif
ifdef DISABLE_SCLP
	ENABLE_SCLP =
endif
ifdef DISABLE_SCHS
	ENABLE_SCHS =
endif
ifdef DISABLE_SAHD
	ENABLE_SAHD =
endif

ifdef ENABLE_SCHD
	CXXFLAGS += -DBUILD_SCHD
endif
ifdef ENABLE_SCMO
	CXXFLAGS += -DBUILD_SCMO
endif
ifdef ENABLE_SCCD
	CXXFLAGS += -DBUILD_SCCD
endif
ifdef ENABLE_SCDP
	CXXFLAGS += -DBUILD_SCDP
endif
ifdef ENABLE_SCLP
	CXXFLAGS += -DBUILD_SCLP
endif
ifdef ENABLE_SCHS
	CXXFLAGS += -DBUILD_SCHS
endif
ifdef ENABLE_SAHD
	CXXFLAGS += -DBUILD_SAHD
endif

ifeq ($(or $(ENABLE_SCHD),$(ENABLE_SCMO),$(ENABLE_SCCD),$(ENABLE_SAHD),$(ENABLE_SCHS)),1)
	CXXFLAGS += -DBUILD_MODE_PAGE_DEVICE
endif

ifeq ($(or $(ENABLE_SCHD),$(ENABLE_SCMO),$(ENABLE_SCCD),$(ENABLE_SAHD)),1)
	CXXFLAGS += -DBUILD_DISK
endif

S2P := s2p
S2PCTL := s2pctl
S2PDUMP := s2pdump
S2PEXEC := s2pexec
S2PPROTO := s2pproto
S2P_TEST := s2p_test
IN_PROCESS_TEST := in_process_test

INSTALL_BIN := $(TARGET)/bin

OBJDIR := obj
LIBDIR := lib
BINDIR := bin

BIN_ALL = \
	$(BINDIR)/$(S2P) \
	$(BINDIR)/$(S2PCTL)

# s2pdump s2pexec and s2pproto require initiator support
ifeq ($(BOARD), FULLSPEC)
	BIN_ALL += $(BINDIR)/$(S2PDUMP)
	BIN_ALL += $(BINDIR)/$(S2PEXEC)
	BIN_ALL += $(BINDIR)/$(S2PPROTO)
endif

DIR_SHARED := shared
DIR_SHARED_PROTOBUF := protobuf
DIR_SHARED_COMMAND := command
DIR_SHARED_INITIATOR := initiator
DIR_BASE := base
DIR_BUSES := buses
DIR_CONTROLLERS := controllers
DIR_DEVICES := devices

SRC_PROTOC := ../proto/s2p_interface.proto

SRC_GENERATED = $(GENERATED_DIR)/s2p_interface.pb.cpp

SRC_SHARED := $(shell ls -1 $(DIR_SHARED)/*.cpp)
SRC_SHARED_PROTOBUF := $(shell ls -1 $(DIR_SHARED_PROTOBUF)/*.cpp)
SRC_SHARED_COMMAND := $(shell ls -1 $(DIR_SHARED_COMMAND)/*.cpp)
SRC_SHARED_INITIATOR := $(shell ls -1 $(DIR_SHARED_INITIATOR)/*.cpp)
SRC_BASE = $(shell ls -1 $(DIR_BASE)/*.cpp)
SRC_BUSES = $(shell ls -1 $(DIR_BUSES)/*.cpp)
SRC_CONTROLLERS = $(shell ls -1 $(DIR_CONTROLLERS)/*.cpp)

SRC_DISK = \
	$(DIR_DEVICES)/disk.cpp \
	$(DIR_DEVICES)/linux_cache.cpp \
	$(DIR_DEVICES)/disk_cache.cpp \
	$(DIR_DEVICES)/disk_track.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/mode_page_device.cpp

ifeq ($(ENABLE_SCHD),1)
SRC_SCHD = \
	$(DIR_DEVICES)/scsi_hd.cpp
SRC_SCHD += $(SRC_DISK)
endif

ifeq ($(ENABLE_SCMO),1)
SRC_SCMO = \
	$(DIR_DEVICES)/optical_memory.cpp
SRC_SCMO += $(SRC_DISK)
endif

ifeq ($(ENABLE_SCCD),1)
SRC_SCCD = \
	$(DIR_DEVICES)/scsi_cd.cpp
SRC_SCCD += $(SRC_DISK)
endif

ifeq ($(ENABLE_SCDP),1)
SRC_SCDP = \
	$(DIR_DEVICES)/daynaport.cpp \
	$(DIR_DEVICES)/tap_driver.cpp
endif

ifeq ($(ENABLE_SCLP),1)
SRC_SCLP = \
	$(DIR_DEVICES)/printer.cpp
endif

ifeq ($(ENABLE_SCHS),1)
SRC_SCHS = \
	$(DIR_DEVICES)/host_services.cpp \
	$(DIR_DEVICES)/mode_page_device.cpp
endif

ifeq ($(ENABLE_SAHD),1)
SRC_SAHD = \
	$(DIR_DEVICES)/sasi_hd.cpp
SRC_SAHD += $(SRC_DISK)
endif

SRC_S2P_CORE = $(shell ls -1 s2p/*.cpp | grep -v s2p.cpp)
SRC_S2P = s2p/s2p.cpp

SRC_S2PCTL_CORE = $(shell ls -1 s2pctl/*.cpp | grep -v s2pctl.cpp)
SRC_S2PCTL = s2pctl/s2pctl.cpp

SRC_S2PDUMP = s2pdump/s2pdump.cpp
SRC_S2PDUMP += $(shell ls -1 s2pdump/*.cpp | grep -v s2pdump.cpp)

SRC_S2PEXEC := $(shell ls -1 s2pexec/*.cpp)

SRC_S2PPROTO := $(shell ls -1 s2pproto/*.cpp)

SRC_S2P_TEST = $(shell ls -1 test/*.cpp | grep -v in_process_test.cpp)
SRC_S2P_TEST += $(shell ls -1 s2pdump/*.cpp | grep -v s2pdump.cpp)

SRC_IN_PROCESS_TEST = test/in_process_test.cpp
SRC_IN_PROCESS_TEST += $(shell ls -1 s2pdump/*.cpp | grep -v s2pdump.cpp)
SRC_IN_PROCESS_TEST += $(shell ls -1 s2pexec/*.cpp | grep -v s2pexec.cpp)
SRC_IN_PROCESS_TEST += $(shell ls -1 s2pproto/*.cpp | grep -v s2pproto.cpp)

VPATH := $(DIR_SHARED) $(DIR_SHARED_PROTOBUF) $(DIR_SHARED_INITIATOR) $(DIR_SHARED_COMMAND) $(DIR_BASE) $(DIR_BUSES) \
	$(DIR_CONTROLLERS) $(DIR_DEVICES) ./s2p ./s2pctl ./s2pdump ./s2pexec ./s2pproto

vpath %.h $(VPATH)
vpath %.cpp $(VPATH) test
vpath %.o $(OBJDIR)

LIB_SHARED := $(LIBDIR)/libshared.a
LIB_SHARED_PROTOBUF := $(LIBDIR)/libprotobuf.a
LIB_SHARED_COMMAND := $(LIBDIR)/libcommand.a
LIB_SHARED_INITIATOR := $(LIBDIR)/libinitiator.a
LIB_BUS := $(LIBDIR)/libbus.a
LIB_CONTROLLER := $(LIBDIR)/libcontroller.a
LIB_DEVICE := $(LIBDIR)/libdevice.a

OBJ_SCHD := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCHD:%.cpp=%.o)))
OBJ_SCMO := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCMO:%.cpp=%.o)))
OBJ_SCCD := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCCD:%.cpp=%.o)))
OBJ_SCDP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCDP:%.cpp=%.o)))
OBJ_SCLP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCLP:%.cpp=%.o)))
OBJ_SCHS := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SCHS:%.cpp=%.o)))
OBJ_SAHD := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SAHD:%.cpp=%.o)))
OBJ_SHARED := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SHARED:%.cpp=%.o)))
OBJ_SHARED_PROTOBUF := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SHARED_PROTOBUF:%.cpp=%.o)))
OBJ_SHARED_COMMAND := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SHARED_COMMAND:%.cpp=%.o)))
OBJ_SHARED_INITIATOR := $(addprefix $(OBJDIR)/,$(notdir $(SRC_SHARED_INITIATOR:%.cpp=%.o)))
OBJ_BASE := $(addprefix $(OBJDIR)/,$(notdir $(SRC_BASE:%.cpp=%.o)))
OBJ_BUSES := $(addprefix $(OBJDIR)/,$(notdir $(SRC_BUSES:%.cpp=%.o)))
OBJ_CONTROLLERS := $(addprefix $(OBJDIR)/,$(notdir $(SRC_CONTROLLERS:%.cpp=%.o)))
OBJ_DEVICES := $(shell echo $(OBJ_SCHD) $(OBJ_SCMO) $(OBJ_SCCD) $(OBJ_SCDP) $(OBJ_SCLP) $(OBJ_SCHS) $(OBJ_SAHD) | xargs -n1 | sort -u | xargs)
OBJ_S2P_CORE := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2P_CORE:%.cpp=%.o)))
OBJ_S2P := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2P:%.cpp=%.o)))
OBJ_S2PCTL_CORE := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2PCTL_CORE:%.cpp=%.o)))
OBJ_S2PCTL := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2PCTL:%.cpp=%.o)))
OBJ_S2PDUMP := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2PDUMP:%.cpp=%.o)))
OBJ_S2PEXEC := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2PEXEC:%.cpp=%.o)))
OBJ_S2PPROTO := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2PPROTO:%.cpp=%.o)))
OBJ_GENERATED := $(addprefix $(OBJDIR)/,$(notdir $(SRC_GENERATED:%.cpp=%.o)))
OBJ_S2P_TEST := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2P_TEST:%.cpp=%.o)))
OBJ_IN_PROCESS_TEST := $(addprefix $(OBJDIR)/,$(notdir $(SRC_IN_PROCESS_TEST:%.cpp=%.o)))


BINARIES = $(INSTALL_BIN)/$(S2PCTL) \
	$(INSTALL_BIN)/$(S2P)
ifeq ($(BOARD), FULLSPEC)
	BINARIES += $(INSTALL_BIN)/$(S2PDUMP)
	BINARIES += $(INSTALL_BIN)/$(S2PEXEC)
	BINARIES += $(INSTALL_BIN)/$(S2PPROTO)
endif

GENERATED_DIR := generated

# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
ALL_DEPS := $(patsubst %.o,%.d,$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P) $(OBJ_S2PCTL) $(OBJ_S2PDUMP) \
	$(OBJ_S2PEXEC) $(OBJ_S2PPROTO) $(OBJ_SHARED) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED_INITIATOR) $(OBJ_SHARED_COMMAND) \
	$(OBJ_BASE) $(OBJ_BUSES) $(OBJ_CONTROLLERS) $(OBJ_DEVICES) $(OBJ_S2P_TEST) $(OBJ_IN_PROCESS_TEST))
-include $(ALL_DEPS)

$(OBJ_GENERATED): $(SRC_GENERATED)
$(LIB_SHARED): $(OBJ_SHARED)
$(LIB_SHARED_PROTOBUF): $(OBJ_GENERATED) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_COMMAND): $(OBJ_SHARED_COMMAND) $(OBJ_GENERATED) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_INITIATOR): $(OBJ_SHARED_INITIATOR)
$(LIB_BUS): $(OBJ_BUSES)
$(LIB_CONTROLLER): $(OBJ_CONTROLLERS)
$(LIB_DEVICE): $(OBJ_DEVICES) $(OBJ_BASE)

$(OBJDIR) $(LIBDIR) $(BINDIR) $(GENERATED_DIR):
	mkdir -p $@

$(LIB_SHARED) $(LIB_SHARED_PROTOBUF) $(LIB_SHARED_INITIATOR) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE): | $(LIBDIR)
	$(AR) rcs $@ $^

ifeq ($(DATABASE), 1)
$(OBJDIR)/%.o: %.cpp | $(OBJDIR) $(SRC_GENERATED)
	$(CXX) $(CXXFLAGS) -c $< -o $@ -MJ $@.json
else
$(OBJDIR)/%.o: %.cpp | $(OBJDIR) $(SRC_GENERATED)
	$(CXX) $(CXXFLAGS) -c $< -o $@
endif

$(OBJ_GENERATED): | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_GENERATED): $(SRC_PROTOC) | $(GENERATED_DIR)
	protoc -I ../proto --cpp_out=$(GENERATED_DIR) $(SRC_PROTOC)
	mv $(GENERATED_DIR)/s2p_interface.pb.cc $@

## Build targets:
##   all             Rebuild all of the executable files (default target)
##   test            Build and run unit tests
##   in_process_test Build the in-process test
##   coverage        Build and run unit tests and create coverage files
##                   Run 'make clean' between coverage and non-coverage builds.
.DEFAULT_GOAL := all
.PHONY: all test coverage

all: cpp

cpp: $(BIN_ALL)

test: $(BINDIR)/$(S2P_TEST)
	$(BINDIR)/$(S2P_TEST)

in_process_test: $(BINDIR)/$(IN_PROCESS_TEST)

coverage: CXXFLAGS += --coverage
coverage: test

$(SRC_S2P_CORE) $(SRC_S2PCTL_CORE) $(SRC_S2PPROTO) $(SRC_IN_PROCESS_TEST): $(OBJ_GENERATED)

$(BINDIR)/$(S2P): $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(OBJ_S2P_CORE) $(OBJ_S2P) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2P) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) -lpthread -lprotobuf

$(BINDIR)/$(S2PCTL): $(LIB_SHARED_PROTOBUF) $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) $(LIB_SHARED_PROTOBUF) -lprotobuf

$(BINDIR)/$(S2PDUMP): $(OBJ_S2PDUMP) $(LIB_SHARED) $(LIB_SHARED_INITIATOR) $(LIB_BUS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PDUMP) $(LIB_SHARED) $(LIB_SHARED_INITIATOR) $(LIB_BUS)

$(BINDIR)/$(S2PEXEC): $(OBJ_S2PEXEC) $(LIB_SHARED) $(LIB_SHARED_INITIATOR) $(LIB_BUS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PEXEC) $(LIB_SHARED) $(LIB_SHARED_INITIATOR) $(LIB_BUS)

$(BINDIR)/$(S2PPROTO): $(OBJ_S2PPROTO) $(LIB_SHARED) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(OBJ_GENERATED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PPROTO) $(LIB_SHARED) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(OBJ_GENERATED) -lprotobuf

$(BINDIR)/$(S2P_TEST): $(LIB_SHARED_COMMAND) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) \
	$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(OBJ_S2PCTL_TEST) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(LIB_SHARED_COMMAND) \
		$(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) -lpthread -lprotobuf -lgmock -lgtest

$(BINDIR)/$(IN_PROCESS_TEST): $(LIB_SHARED_COMMAND) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) \
	$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_IN_PROCESS_TEST) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_IN_PROCESS_TEST) $(LIB_SHARED_COMMAND) \
		$(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) -lpthread -lprotobuf

# Phony rules for building individual utilities
.PHONY: $(S2P) $(S2PCTL) $(S2PDUMP) $(S2PEXEC) $(S2PPROTO) $(S2P_TEST) $(IN_PROCESS_TEST)

$(S2P): $(BINDIR)/$(S2P) 
$(S2PCTL): $(BINDIR)/$(S2PCTL) 
$(S2PDUMP): $(BINDIR)/$(S2PDUMP) 
$(S2PEXEC): $(BINDIR)/$(S2PEXEC) 
$(S2PPROTO): $(BINDIR)/$(S2PPROTO) 
$(S2P_TEST): $(BINDIR)/$(S2P_TEST)
$(IN_PROCESS_TEST): $(BINDIR)/$(IN_PROCESS_TEST)

##   clean           Remove all of the object files, intermediate 
##                   compiler files and executable files 
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR) $(GENERATED_DIR)

##   install         Installs the stripped binaries, usually to /opt/s2p/bin
.PHONY: install
install: $(BINARIES)
	$(STRIP) $^
	chown root:root $^
	chmod +s $^

$(INSTALL_BIN)%: $(BINDIR)/%
	mkdir -p $(INSTALL_BIN)
	cp $< $@
	


##   help            Lists information about how to use the Makefile
# The help rule is based upon the approach from:
#   https://swcarpentry.github.io/make-novice/08-self-doc/index.html 
.PHONY: help
help: Makefile
	@sed -n 's/^##//p' $<

