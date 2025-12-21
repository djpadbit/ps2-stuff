# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

# Executable
EE_BIN = testing.elf

# EE src files & include dir
EE_SRC_DIR := src
EE_VU_SRC_DIR := vu
EE_INCLUDE_DIR := include

RESOURCE_DIR := resources

# Flags
EE_CFLAGS := 
EE_LIBS = -ldraw -lgraph -lmath3d -lpacket2 -ldma -lpad

# General obj dir
OBJ_DIR := objs

# Should we keep VSM output
KEEP_VSM := true

# For ps2client
PS2_HOST := 192.168.69.2

# ---------------------------------

# Command for ensuring the output directory for the rule exists.
DIR_GUARD = @$(MKDIR) -p $(@D)

# VU Tools
EE_DVP = dvp-as
EE_VCL = vcl

EE_INCS := -I$(EE_INCLUDE_DIR)/

EE_SRC_FILES := $(shell find $(EE_SRC_DIR)/ -type f -name '*.c' -not -path '*/$(RESOURCE_DIR)/*')
EE_VU_SRC_FILES := $(shell find $(EE_VU_SRC_DIR)/ -type f -name '*.vcl')
RESOURCES_FILES := $(shell find $(RESOURCE_DIR)/ -type f -name '*.raw')

RES_CONV := $(patsubst %.raw,$(EE_SRC_DIR)/%.c,$(RESOURCES_FILES))
EE_OBJS := $(patsubst %.c,$(OBJ_DIR)/ee/%.o,$(EE_SRC_FILES)) $(patsubst %.c,$(OBJ_DIR)/ee/%.o,$(RES_CONV))
EE_DEPS := $(patsubst %.o,$(OBJ_DIR)/ee/%.d,$(EE_OBJS))
EE_OBJS += $(patsubst %.vcl,$(OBJ_DIR)/vu/%.o,$(EE_VU_SRC_FILES))
EE_VU_VSM_FILES := $(patsubst %.vcl,%.vsm,$(EE_VU_SRC_FILES))

.DEFAULT_GOAL := $(EE_BIN)

-include $(EE_DEPS)

# Original VCL tool preferred. 
# It can be runned on WSL, but with some tricky commands: 
# https://github.com/microsoft/wsl/issues/2468#issuecomment-374904520
%.vsm: %.vcl Makefile
	$(DIR_GUARD)
	$(EE_VCL) -t10 $< > $@

ifeq ($(KEEP_VSM), true)
.PRECIOUS: $(EE_VU_VSM_FILES)
endif

$(OBJ_DIR)/vu/%.o: %.vsm
	$(DIR_GUARD)
	$(EE_DVP) $< -o $@

$(EE_SRC_DIR)/$(RESOURCE_DIR)/%.c: $(RESOURCE_DIR)/%.raw
	$(DIR_GUARD)
	bin2c $< $@ $(notdir $*)

.PHONY: clean cleanc
clean:
	rm -rf $(OBJ_DIR) $(EE_BIN) $(RES_CONV) $(EE_VU_VSM_FILES)

cleanc:
	rm -rf $(OBJ_DIR) $(EE_BIN) $(RES_CONV)

run: $(EE_BIN)
	ps2client -h $(PS2_HOST) execee host:$(EE_BIN)

reset:
	ps2client -h $(PS2_HOST) reset

# ---------------------------------

include $(PS2SDK)/Defs.make

# Helpers to make easy the use of newlib-nano
NODEFAULTLIBS = 0

LIBC = -lc
LIBM = -lm
ifeq ($(NEWLIB_NANO), 1)
   NODEFAULTLIBS = 1
   LIBC = -lc_nano
   LIBM = -lm_nano
endif

EXTRA_LDFLAGS =
ifeq ($(NODEFAULTLIBS), 1)
   EXTRA_LDFLAGS = -nodefaultlibs $(LIBM) -lgcc -Wl,--start-group $(LIBC) -lcdvd -lcglue -lpthread -lpthreadglue -lkernel -Wl,--end-group
endif

# Include directories
EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. $(EE_INCS)

# Optimization compiler flags
EE_OPTFLAGS ?= -O2

# Warning compiler flags
EE_WARNFLAGS ?= -Wall

# Debug information flags
EE_DBGINFOFLAGS ?= -gdwarf-2 -gz

# C compiler flags
EE_CFLAGS := -D_EE $(EE_OPTFLAGS) $(EE_WARNFLAGS) $(EE_DBGINFOFLAGS) $(EE_CFLAGS)

# Linker flags
EE_LDFLAGS := -L$(PS2SDK)/ee/lib -Wl,-zmax-page-size=128 $(EE_LDFLAGS)

# Default link file
ifeq ($(EE_LINKFILE),)
EE_LINKFILE := $(PS2SDK)/ee/startup/linkfile
endif

# Externally defined variables: EE_BIN, EE_OBJS, EE_LIBS

# These macros can be used to simplify certain build rules.
EE_C_COMPILE = $(EE_CC) $(EE_CFLAGS) $(EE_INCS)
EE_CXX_COMPILE = $(EE_CXX) $(EE_CXXFLAGS) $(EE_INCS)

$(OBJ_DIR)/ee/%.o: %.c Makefile
	$(DIR_GUARD)
	$(EE_CC) -MMD -MP $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(EE_BIN): $(RES_CONV) $(EE_OBJS) Makefile
	$(DIR_GUARD)
	$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) $(EE_LIBS)
	#$(EE_STRIP) --strip-all $(EE_BIN)