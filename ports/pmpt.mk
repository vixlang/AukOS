# pmpt.mk — AukOS Port Build Rules
# Include in port Makefiles with: include $(ROOT)/ports/pmpt.mk
#
# Provides:
#   PORT_CC, PORT_LD, PORT_AR        — cross-compiler toolchain
#   PORT_CFLAGS, PORT_CPPFLAGS       — AukOS compilation flags
#   PORT_LIBC_OBJS                   — AukOS libc objects for linking
#   port_objects function             — generates object paths from sources
#   Pattern rules: %.c → %.o          — cross-compilation rules

PORT_CC      ?= clang
PORT_LD      ?= ld.lld
PORT_AR      ?= llvm-ar

PORT_CFLAGS  ?= -target x86_64-unknown-none -std=gnu17 -ffreestanding \
	-fno-stack-protector -fno-pic -mcmodel=large -mno-red-zone \
	-fwrapv -fno-common -ffunction-sections -fdata-sections -Oz \
	-Wall -Wextra

PORT_LIBC_OBJS := \
	$(ROOT)/build/toybox-libc/crt0.o \
	$(ROOT)/build/toybox-libc/posix.o \
	$(ROOT)/build/toybox-libc/string.o \
	$(ROOT)/build/toybox-libc/stdio.o \
	$(ROOT)/build/toybox-libc/stdlib.o \
	$(ROOT)/build/toybox-libc/allocator.o \
	$(ROOT)/build/toybox-libc/ctype.o \
	$(ROOT)/build/toybox-libc/environment.o \
	$(ROOT)/build/toybox-libc/libgen.o \
	$(ROOT)/build/toybox-libc/mkstemp.o \
	$(ROOT)/build/toybox-libc/time.o \
	$(ROOT)/build/toybox-libc/system.o \
	$(ROOT)/build/toybox-libc/compat.o \
	$(ROOT)/build/toybox-libc/access.o \
	$(ROOT)/build/toybox-libc/stubs.o

# Convert source paths to object paths.
# Usage:  $(eval $(call port_objects,$(SOURCES)))
# Result: AUKOS_OBJECTS = build/obj/foo.o build/obj/bar.o ...
define port_objects
AUKOS_OBJECTS := $$(addprefix $$(AUKOS_BUILD)/,$$(patsubst %.c,%.o,$$(filter %.c,$1)))
endef

# Compile rule: source from AUKOS_SOURCE_DIR → object
$(AUKOS_BUILD)/%.o: $(AUKOS_SOURCE_DIR)/%.c $(PORT)/config/config.h
	@mkdir -p $(dir $@)
	$(Q)$(PORT_CC) $(PORT_CFLAGS) $(PORT_CPPFLAGS) -I$(PORT) \
		-I$(AUKOS_SOURCE_DIR) -I$(ROOT)/user/include -c $< -o $@
