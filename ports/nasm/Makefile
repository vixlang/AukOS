ROOT := ../..
SOURCE := $(ROOT)/user/nasm
PORT := $(ROOT)/ports/nasm
HOST_ROOT := $(ROOT)/build/nasm-host
HOST_SOURCE := $(HOST_ROOT)/source
HOST_BUILD := $(HOST_ROOT)/obj
HOST_NASM := $(HOST_ROOT)/nasm
HOST_REFERENCE := $(HOST_ROOT)/hello.o
AUKOS_BUILD := $(ROOT)/build/nasm-aukos
AUKOS_SOURCE := $(AUKOS_BUILD)/source
AUKOS_NASM := $(AUKOS_BUILD)/nasm.elf

include $(PORT)/sources.mk

CC := clang
LD := ld.lld
AR := llvm-ar

UPSTREAM_DIRS := asm autoconf common config include macros nasmlib output \
	perllib stdlib tools x86 zlib
UPSTREAM_FILES := $(shell find $(addprefix $(SOURCE)/,$(UPSTREAM_DIRS)) \
	-type f -print) $(shell find $(SOURCE) -maxdepth 1 -type f -print)
HOST_GENERATED := $(addprefix $(HOST_BUILD)/,$(NASM_GENERATED_SOURCES)) \
	$(HOST_BUILD)/asm/directiv.h $(HOST_BUILD)/asm/pptok.h \
	$(HOST_BUILD)/asm/tokens.h $(HOST_BUILD)/asm/warnings_c.h \
	$(HOST_BUILD)/include/warnings.h $(HOST_BUILD)/version.h

NASM_CPPFLAGS := -DHAVE_CONFIG_H -D__AUKOS__ -DOF_ONLY -DOF_ELF64 \
	-DOF_DEFAULT=of_elf64 -DPATHSTYLE=1 -I$(PORT) -I$(AUKOS_SOURCE) -I$(HOST_BUILD) \
	-I$(AUKOS_SOURCE)/include -I$(HOST_BUILD)/include \
	-I$(AUKOS_SOURCE)/x86 -I$(HOST_BUILD)/x86 \
	-I$(AUKOS_SOURCE)/asm -I$(HOST_BUILD)/asm \
	-I$(AUKOS_SOURCE)/output -I$(HOST_BUILD)/output -I$(AUKOS_SOURCE)/zlib \
	-I$(ROOT)/user/include
NASM_CFLAGS := -target x86_64-unknown-none -std=gnu17 -ffreestanding \
	-fno-stack-protector -fno-pic -mcmodel=large -mno-red-zone \
	-fwrapv -fno-common -ffunction-sections -fdata-sections -Oz \
	-Wall -Wextra

NASM_OBJECTS := $(addprefix $(AUKOS_BUILD)/,$(NASM_SOURCES:.c=.o))
NASM_COPIED_SOURCES := $(addprefix $(AUKOS_SOURCE)/, \
	$(filter-out $(NASM_GENERATED_SOURCES),$(NASM_SOURCES)))
NASM_LIBC_OBJECTS := $(addprefix $(ROOT)/build/toybox-libc/, \
	crt0.o posix.o string.o stdio.o stdlib.o allocator.o ctype.o \
	environment.o libgen.o mkstemp.o time.o system.o compat.o access.o stubs.o)

.PHONY: host reference compact-test aukos

host: $(HOST_NASM)

reference: $(HOST_REFERENCE)

compact-test: $(AUKOS_BUILD)/generated/x86/insnsa.c
	@python3 $(PORT)/test_compact_insns.py \
		$(HOST_BUILD)/x86/insnsa.c $<

aukos: $(AUKOS_NASM)

$(HOST_SOURCE)/.prepared: $(UPSTREAM_FILES)
	@command -v autoconf >/dev/null || { echo 'error: autoconf is required for NASM'; exit 1; }
	@command -v perl >/dev/null || { echo 'error: perl is required for NASM tables'; exit 1; }
	@mkdir -p $(HOST_SOURCE)
	@cp -a $(SOURCE)/. $(HOST_SOURCE)/
	@cd $(HOST_SOURCE) && ./autogen.sh
	@touch $@

$(HOST_BUILD)/Makefile: $(HOST_SOURCE)/.prepared
	@mkdir -p $(HOST_BUILD)
	@cd $(HOST_BUILD) && ../source/configure

$(HOST_NASM): $(HOST_BUILD)/Makefile
	@$(MAKE) -C $(HOST_BUILD) nasm
	@cp $(HOST_BUILD)/nasm $@

$(HOST_REFERENCE): $(HOST_NASM) $(PORT)/tests/hello.asm \
		$(PORT)/check_reference.sh
	@$(HOST_NASM) -f elf64 $(PORT)/tests/hello.asm -o $@
	@sh $(PORT)/check_reference.sh $@

$(HOST_GENERATED): $(HOST_NASM)
	@test -f $@

$(AUKOS_SOURCE)/.prepared: $(UPSTREAM_FILES) $(PORT)/patches/compact-insns.patch
	@mkdir -p $(AUKOS_SOURCE)
	@cp -a $(SOURCE)/. $(AUKOS_SOURCE)/
	@patch -d $(AUKOS_SOURCE) -p1 < $(PORT)/patches/compact-insns.patch
	@touch $@

$(NASM_COPIED_SOURCES): $(AUKOS_SOURCE)/.prepared
	@test -f $@

$(AUKOS_BUILD)/generated/x86/insnsa.c: $(HOST_BUILD)/x86/insnsa.c \
		$(PORT)/compact_insns.py
	@python3 $(PORT)/compact_insns.py $< $@

$(AUKOS_BUILD)/x86/insnsa.o: $(AUKOS_BUILD)/generated/x86/insnsa.c \
		$(HOST_GENERATED) $(AUKOS_SOURCE)/.prepared $(PORT)/config/config.h
	@mkdir -p $(dir $@)
	@$(CC) $(NASM_CFLAGS) $(NASM_CPPFLAGS) -c $< -o $@

$(AUKOS_BUILD)/%.o: $(HOST_BUILD)/%.c $(HOST_GENERATED) \
		$(AUKOS_SOURCE)/.prepared $(PORT)/config/config.h
	@mkdir -p $(dir $@)
	@$(CC) $(NASM_CFLAGS) $(NASM_CPPFLAGS) -c $< -o $@

$(AUKOS_BUILD)/%.o: $(AUKOS_SOURCE)/%.c $(HOST_GENERATED) \
		$(AUKOS_SOURCE)/.prepared $(PORT)/config/config.h
	@mkdir -p $(dir $@)
	@$(CC) $(NASM_CFLAGS) $(NASM_CPPFLAGS) -c $< -o $@

$(AUKOS_NASM): $(NASM_OBJECTS) $(NASM_LIBC_OBJECTS) $(ROOT)/user/linker.ld
	@$(LD) -nostdlib -static --gc-sections -T $(ROOT)/user/linker.ld \
		-o $@ $(NASM_OBJECTS) $(NASM_LIBC_OBJECTS)
