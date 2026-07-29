# Port: curl — HTTP/HTTPS/FTP client
# NOTE: Requires TCP stack in kernel. AukOS currently has UDP only,
#       so this port is a framework placeholder until SOCK_STREAM
#       is implemented in kernel/syscall.c.
ROOT := ../..
SOURCE := $(ROOT)/user/curl
PORT := $(ROOT)/ports/curl
HOST_BUILD := $(ROOT)/build/curl-host
AUKOS_BUILD := $(ROOT)/build/curl-aukos
AUKOS_SOURCE_DIR := $(AUKOS_BUILD)/source
AUKOS_BIN := $(AUKOS_BUILD)/curl.elf

CURL_LIBS := $(AUKOS_BUILD)/lib/libcurl.a
CURL_CFLAGS := -DHAVE_CONFIG_H -DBUILDING_LIBCURL -DCURL_STATICLIB \
	-DHAVE_CONFIG_H -I$(PORT)/config -I$(AUKOS_SOURCE_DIR)/include \
	-I$(AUKOS_SOURCE_DIR)/lib -I$(ROOT)/user/include

include $(PORT)/sources.mk
include $(ROOT)/ports/pmpt.mk

# curl-specific additions to base flags
PORT_CPPFLAGS := $(CURL_CFLAGS)

# Build host-side curl tooling (tool_hugehelp, etc.) if needed
# ...

# Libcurl archive
$(CURL_LIBS): $(AUKOS_OBJECTS)
	@mkdir -p $(dir $@)
	$(Q)$(PORT_AR) cr $@ $(AUKOS_OBJECTS)

# curl CLI binary (links libcurl + main tool)
$(AUKOS_BIN): $(AUKOS_OBJECTS) $(PORT_LIBC_OBJS) $(ROOT)/user/linker.ld
	$(Q)$(PORT_LD) -nostdlib -static --gc-sections \
		-T $(ROOT)/user/linker.ld -o $@ \
		$(AUKOS_OBJECTS) $(PORT_LIBC_OBJS)

.PHONY: aukos clean

aukos: $(AUKOS_BIN)

clean:
	rm -rf $(AUKOS_BUILD)

# Pull upstream source into build tree (with AukOS patches)
$(AUKOS_SOURCE_DIR)/.prepared: $(shell find $(SOURCE) -type f 2>/dev/null) \
		$(PORT)/patches/*.patch
	@mkdir -p $(AUKOS_SOURCE_DIR)
	@cp -a $(SOURCE)/. $(AUKOS_SOURCE_DIR)/
	@for p in $(PORT)/patches/*.patch; do \
		[ -f "$$p" ] && patch -d $(AUKOS_SOURCE_DIR) -p1 < "$$p" || true; \
	done
	@touch $@
