.DEFAULT_GOAL: all

## Optional build flags:
##   CROSS_COMPILE= Specifies the compiler toolchain, e.g.
##                  arm-linux-gnueabihf-
CROSS_COMPILE =

CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)ar
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
	CXXFLAGS += -fexperimental-library -I/usr/local/include -Wno-unused-parameter
endif

ifeq ("$(shell uname -s)", "NetBSD")
	CXXFLAGS += -I/usr/pkg/include -Wno-unused-parameter -Wno-macro-redefined
endif

## EXTRA_FLAGS Can be used to pass special purpose flag
CXXFLAGS += $(EXTRA_FLAGS)

## Installation path
##   The path to install binaries and manpages in, default is /opt/scsi2pi
TARGET=/opt/scsi2pi

##   BOARD=FULLSPEC
##     Specifies the board type that you are using. Typically this is 
##     STANDARD or FULLSPEC. The default is FULLSPEC.
BOARD ?= FULLSPEC

CXXFLAGS += -DBOARD_$(BOARD)

##   BUILD_*=1
##     Specifies the device types to build, default is all. Supported types:
##       SCHD SCSI Removable and non-removable Hard Drive
##       SCCD SCSI CD-Rom Drive
##       SCMO SCSI Optical Memory
##       SCDP DaynaPort Network Adapter
##       SCLP SCSI Printer
##       SCHS Host Services
##       SAHD SASI Hard Drive
ifeq ($(or $(BUILD_SCHD),$(BUILD_SCMO),$(BUILD_SCCD),$(BUILD_SCDP),$(BUILD_SCLP),$(BUILD_SCHS),$(BUILD_SAHD)),)
	BUILD_SCHD = 1
	BUILD_SCMO = 1
	BUILD_SCCD = 1
	BUILD_SCDP = 1
	BUILD_SCLP = 1
	BUILD_SCHS = 1
	BUILD_SAHD = 1
endif
ifdef BUILD_SCHD
	CXXFLAGS += -DBUILD_MODE_PAGE_DEVICE -DBUILD_DISK -DBUILD_SCHD
endif
ifdef BUILD_SCMO
	CXXFLAGS += -DBUILD_MODE_PAGE_DEVICE -DBUILD_DISK -DBUILD_SCMO
endif
ifdef BUILD_SCCD
	CXXFLAGS += -DBUILD_MODE_PAGE_DEVICE -DBUILD_DISK -DBUILD_SCCD
endif
ifdef BUILD_SCDP
	CXXFLAGS += -DBUILD_SCDP
endif
ifdef BUILD_SCLP
	CXXFLAGS += -DBUILD_SCLP
endif
ifdef BUILD_SCHS
	CXXFLAGS += -DBUILD_MODE_PAGE_DEVICE -DBUILD_SCHS
endif
ifdef BUILD_SAHD
	CXXFLAGS += -DBUILD_DISK -DBUILD_SAHD
endif

S2P = s2p
S2PCTL = s2pctl
S2PDUMP = s2pdump
S2PEXEC = s2pexec
S2P_TEST = s2p_test
IN_PROCESS_TEST = in_process_test

INSTALL_BIN = $(TARGET)/bin

OBJDIR := obj
LIBDIR := lib
BINDIR := bin

BIN_ALL = \
	$(BINDIR)/$(S2P) \
	$(BINDIR)/$(S2PCTL)

# s2pdump and s2pexec require initiator support
ifeq ($(BOARD), FULLSPEC)
	BIN_ALL += $(BINDIR)/$(S2PDUMP)
	BIN_ALL += $(BINDIR)/$(S2PEXEC)
endif

DIR_SHARED := shared
DIR_SHARED_PROTOBUF := shared_protobuf
DIR_SHARED_COMMAND := shared_command
DIR_BASE := base
DIR_BUSES := buses
DIR_CONTROLLERS := controllers
DIR_DEVICES := devices

SRC_PROTOC = ../proto/s2p_interface.proto

SRC_GENERATED = $(GENERATED_DIR)/s2p_interface.pb.cpp

SRC_SHARED := $(shell ls -1 $(DIR_SHARED)/*.cpp)
SRC_SHARED_PROTOBUF := $(shell ls -1 $(DIR_SHARED_PROTOBUF)/*.cpp)
SRC_SHARED_COMMAND := $(shell ls -1 $(DIR_SHARED_COMMAND)/*.cpp)
SRC_BASE = $(shell ls -1 $(DIR_BASE)/*.cpp)
SRC_BUSES = $(shell ls -1 $(DIR_BUSES)/*.cpp)
SRC_CONTROLLERS = $(shell ls -1 $(DIR_CONTROLLERS)/*.cpp)

SRC_DISK = \
	$(DIR_DEVICES)/disk.cpp \
	$(DIR_DEVICES)/disk_cache.cpp \
	$(DIR_DEVICES)/disk_track.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/mode_page_device.cpp \
	$(DIR_DEVICES)/mode_page_util.cpp

ifdef BUILD_SCHD
SRC_SCHD = \
	$(DIR_DEVICES)/scsi_hd.cpp
SRC_SCHD += $(SRC_DISK)
endif

ifdef BUILD_SCMO
SRC_SCMO = \
	$(DIR_DEVICES)/optical_memory.cpp
SRC_SCMO += $(SRC_DISK)
endif

ifdef BUILD_SCCD
SRC_SCCD = \
	$(DIR_DEVICES)/scsi_cd.cpp \
	$(DIR_DEVICES)/cd_track.cpp
SRC_SCCD += $(SRC_DISK)
endif

ifdef BUILD_SCDP
SRC_SCDP = \
	$(DIR_DEVICES)/daynaport.cpp \
	$(DIR_DEVICES)/ctapdriver.cpp
endif

ifdef BUILD_SCLP
SRC_SCLP = \
	$(DIR_DEVICES)/printer.cpp
endif

ifdef BUILD_SCHS
SRC_SCHS = \
	$(DIR_DEVICES)/host_services.cpp \
	$(DIR_DEVICES)/mode_page_device.cpp
endif

ifdef BUILD_SAHD
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

SRC_S2P_TEST = $(shell ls -1 test/*.cpp | grep -v in_process_test.cpp)
SRC_S2P_TEST += $(shell ls -1 s2pdump/*.cpp | grep -v s2pdump.cpp)

SRC_IN_PROCESS_TEST = test/in_process_test.cpp
SRC_IN_PROCESS_TEST += $(shell ls -1 s2pdump/*.cpp | grep -v s2pdump.cpp)
SRC_IN_PROCESS_TEST += $(shell ls -1 s2pexec/*.cpp | grep -v s2pexec.cpp)

VPATH := $(DIR_SHARED) $(DIR_SHARED_PROTOBUF) $(DIR_SHARED_COMMAND) $(DIR_BASE) $(DIR_BUSES) $(DIR_CONTROLLERS) \
	$(DIR_DEVICES) ./s2p ./s2pctl ./s2pdump ./s2pexec

vpath %.h $(VPATH)
vpath %.cpp $(VPATH) test
vpath %.o $(OBJDIR)

LIB_SHARED := $(LIBDIR)/libshared.a
LIB_SHARED_PROTOBUF := $(LIBDIR)/libsharedprotobuf.a
LIB_SHARED_COMMAND := $(LIBDIR)/libsharedcommand.a
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
OBJ_GENERATED := $(addprefix $(OBJDIR)/,$(notdir $(SRC_GENERATED:%.cpp=%.o)))
OBJ_S2P_TEST := $(addprefix $(OBJDIR)/,$(notdir $(SRC_S2P_TEST:%.cpp=%.o)))
OBJ_IN_PROCESS_TEST := $(addprefix $(OBJDIR)/,$(notdir $(SRC_IN_PROCESS_TEST:%.cpp=%.o)))


BINARIES = $(INSTALL_BIN)/$(S2PCTL) \
	$(INSTALL_BIN)/$(S2P)
ifeq ($(BOARD), FULLSPEC)
	BINARIES += $(INSTALL_BIN)/$(S2PDUMP)
	BINARIES += $(INSTALL_BIN)/$(S2PEXEC)
endif

GENERATED_DIR := generated

# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
ALL_DEPS := $(patsubst %.o,%.d,$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P) $(OBJ_S2PCTL) $(OBJ_S2PDUMP) \
	$(OBJ_S2PEXEC) $(OBJ_SHARED) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED_COMMAND) $(OBJ_BASE) $(OBJ_BUSES) \
	$(OBJ_CONTROLLERS) $(OBJ_DEVICES) $(OBJ_S2P_TEST) $(OBJ_IN_PROCESS_TEST))
-include $(ALL_DEPS)

$(OBJ_GENERATED): $(SRC_GENERATED)
$(LIB_SHARED): $(OBJ_SHARED)
$(LIB_SHARED_PROTOBUF): $(OBJ_GENERATED) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_COMMAND): $(OBJ_SHARED_COMMAND) $(OBJ_GENERATED) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_BUS): $(OBJ_BUSES)
$(LIB_CONTROLLER): $(OBJ_CONTROLLERS)
$(LIB_DEVICE): $(OBJ_DEVICES) $(OBJ_BASE)

$(OBJDIR) $(LIBDIR) $(BINDIR) $(GENERATED_DIR):
	mkdir -p $@

$(LIB_SHARED) $(LIB_SHARED_PROTOBUF) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE): | $(LIBDIR)
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

$(SRC_S2P_CORE) $(SRC_S2PCTL_CORE) $(SRC_S2PEXEC): $(OBJ_GENERATED)

$(BINDIR)/$(S2P): $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(OBJ_S2P_CORE) $(OBJ_S2P) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2P) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) -lpthread -lpcap -lprotobuf

$(BINDIR)/$(S2PCTL): $(LIB_SHARED_PROTOBUF) $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) $(LIB_SHARED_PROTOBUF) -lprotobuf

$(BINDIR)/$(S2PDUMP): $(OBJ_S2PDUMP) $(LIB_SHARED) $(LIB_BUS) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PDUMP) $(LIB_SHARED) $(LIB_BUS)

$(BINDIR)/$(S2PEXEC): $(OBJ_S2PEXEC) $(LIB_SHARED) $(LIB_BUS) $(OBJ_GENERATED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PEXEC) $(LIB_SHARED) $(LIB_BUS) $(OBJ_GENERATED) -lprotobuf

$(BINDIR)/$(S2P_TEST): $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(OBJ_S2PCTL_TEST) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) -lpthread -lpcap -lprotobuf -lgmock -lgtest

$(BINDIR)/$(IN_PROCESS_TEST): $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(OBJ_S2P_CORE) $(OBJ_IN_PROCESS_TEST) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_IN_PROCESS_TEST) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) -lpthread -lpcap -lprotobuf

# Phony rules for building individual utilities
.PHONY: s2p $(S2P) $(S2PCTL) $(S2PDUMP) $(S2PEXEC) $(S2P_TEST) $(IN_PROCESS_TEST)

s2p: $(BINDIR)/$(S2P)
$(S2P): $(BINDIR)/$(S2P) 
$(S2PCTL): $(BINDIR)/$(S2PCTL) 
$(S2PDUMP): $(BINDIR)/$(S2PDUMP) 
$(S2PEXEC): $(BINDIR)/$(S2PEXEC) 
$(S2P_TEST): $(BINDIR)/$(S2P_TEST)
$(IN_PROCESS_TEST): $(BINDIR)/$(IN_PROCESS_TEST)

##   clean           Remove all of the object files, intermediate 
##                   compiler files and executable files 
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR) $(GENERATED_DIR)

##   install         Installs the stripped binaries, usually to /usr/local/bin
.PHONY: install
install: $(BINARIES)
	$(STRIP) $^

$(INSTALL_BIN)%: $(BINDIR)/%
	mkdir -p $(INSTALL_BIN)
	cp $< $@

##   help            Lists information about how to use the Makefile
# The help rule is based upon the approach from:
#   https://swcarpentry.github.io/make-novice/08-self-doc/index.html 
.PHONY: help
help: Makefile
	@sed -n 's/^##//p' $<

