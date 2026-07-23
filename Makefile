CONFIG_MK ?= config.mk
include $(CONFIG_MK)

ISO_BIOS_DIR := $(BUILD_DIR)/iso-bios
ISO_UEFI_DIR := $(BUILD_DIR)/iso-uefi
HOST_TEST_DIR := $(BUILD_DIR)/host-tests
USER_CHECK_DIR := $(BUILD_DIR)/user-check
USER_DIR := $(BUILD_DIR)/user
KERNEL := $(BUILD_DIR)/aukos.elf
KERNEL_UEFI := $(BUILD_DIR)/aukos_uefi.elf
BOOTLOADER_ELF := $(BUILD_DIR)/bootloader.elf
BOOTLOADER_EFI := $(BUILD_DIR)/bootloader.efi
ISO := $(BUILD_DIR)/aukos.iso
ISO_UEFI := $(BUILD_DIR)/aukos_uefi.iso
USER_HELLO := $(USER_DIR)/hello.elf
USER_AUSH := $(USER_DIR)/aush.elf
USER_WRITE_TEST := $(USER_DIR)/write_test.elf
USER_SIGNAL_TEST := $(USER_DIR)/signal_test.elf
USER_UDP_TEST := $(USER_DIR)/udp_test.elf
USER_DEV_MEMORY_TEST := $(USER_DIR)/dev_memory_test.elf
USER_PIPE_TEST := $(USER_DIR)/pipe_test.elf
USER_PROCESS_ENV_TEST := $(USER_DIR)/process_env_test.elf
USER_PROCESS_STACK_TEST := $(USER_DIR)/process_stack_test.elf
USER_FILE_API_TEST := $(USER_DIR)/file_api_test.elf
USER_VIX_HELLO := $(USER_DIR)/language_hello.elf
USER_VIX_RUNTIME_TEST := $(USER_DIR)/language_runtime_test.elf
USER_ED := $(USER_DIR)/ed.elf
USER_TOUCH := $(USER_DIR)/touch.elf
USER_VIXC := $(USER_DIR)/vixc.elf
USER_VIXC_TEST := $(USER_DIR)/vixc_test.elf
USER_PERSISTENCE_TEST := $(USER_DIR)/persistence_test.elf
USER_NASM_TEST := $(USER_DIR)/nasm_test.elf
USER_VIX_HELLO_OBJ := $(USER_DIR)/hello.vix.o
USER_ED_OBJ := $(USER_DIR)/ed.vix.o
USER_TOUCH_OBJ := $(USER_DIR)/touch.vix.o
VIX_RUNTIME_OBJECT := $(BUILD_DIR)/vix-runtime/runtime.o
VIX_RUNTIME_BLOB := $(BUILD_DIR)/user/runtime_reloc.o
USER_ED_BLOB := $(BUILD_DIR)/user/ed.o
USER_TOUCH_BLOB := $(BUILD_DIR)/user/touch.o
USER_VIXC_BLOB := $(BUILD_DIR)/user/vixc.o
USER_VIXC_TEST_BLOB := $(BUILD_DIR)/user/vixc_test.o
USER_PERSISTENCE_TEST_BLOB := $(BUILD_DIR)/user/persistence_test.o
USER_NASM_TEST_BLOB := $(BUILD_DIR)/user/nasm_test.o
NASM_HOST := $(BUILD_DIR)/nasm-host/nasm
NASM_AUKOS := $(BUILD_DIR)/nasm-aukos/nasm.elf
USER_NASM_BLOB := $(BUILD_DIR)/user/nasm.o
NASM_SOURCE_DIRS := asm autoconf common config include macros nasmlib output \
	perllib stdlib tools x86 zlib
NASM_UPSTREAM_FILES := $(shell find $(addprefix user/nasm/,$(NASM_SOURCE_DIRS)) \
	-type f -print) $(shell find user/nasm -maxdepth 1 -type f -print)
USER_AUSH_PARSE_OBJ := $(USER_DIR)/shell_parse.o
SIGNAL_TEST_LIBC_OBJ := $(BUILD_DIR)/signal-test-libc/system.o
TOYBOX_HOST := $(TOYBOX_DIR)/toybox
TOYBOX_AUKOS_BIN := $(BUILD_DIR)/toybox-aukos/toybox.elf
TOYBOX_AUKOS_CONFIG := $(TOYBOX_DIR)/.config.aukos
VIRTIO_FIXTURE_GEN := $(BUILD_DIR)/tools/mkext4_fixture
VIRTIO_DISK := $(BUILD_DIR)/virtio-ext4.img
WORK_BASE_DISK := $(BUILD_DIR)/aukos-work-base.img
WORK_DISK := $(BUILD_DIR)/aukos-work.img
UDP_ECHO_SERVER := $(BUILD_DIR)/tools/udp_echo_server
UDP_PCAP_CHECK := $(BUILD_DIR)/tools/check_udp_pcap
VGA_FONT_SOURCE := assets/vga8x16.bin.gz.b64
VGA_FONT_BIN := $(BUILD_DIR)/vga8x16.bin
VGA_FONT_OBJ := $(BUILD_DIR)/vga8x16.o
QEMU_NET_ARGS := -netdev user,id=net0,net=10.0.2.0/24,dhcpstart=10.0.2.15 \
	-device virtio-net-pci,netdev=net0,disable-modern=on,mac=52:54:00:12:34:56

TOYBOX_AUKOS_CFLAGS := -target x86_64-unknown-none -std=gnu17 -ffreestanding -fno-stack-protector -fno-pic -mcmodel=large -mno-red-zone -nostdinc -D__linux__ -I$(CURDIR)/user/include -I$(CURDIR)/kernel/include

TOYBOX_LIBC_OBJS := \
	$(BUILD_DIR)/toybox-libc/crt0.o \
	$(BUILD_DIR)/toybox-libc/posix.o \
	$(BUILD_DIR)/toybox-libc/string.o \
	$(BUILD_DIR)/toybox-libc/stdio.o \
	$(BUILD_DIR)/toybox-libc/stdlib.o \
	$(BUILD_DIR)/toybox-libc/allocator.o \
	$(BUILD_DIR)/toybox-libc/ctype.o \
	$(BUILD_DIR)/toybox-libc/environment.o \
	$(BUILD_DIR)/toybox-libc/libgen.o \
	$(BUILD_DIR)/toybox-libc/mkstemp.o \
	$(BUILD_DIR)/toybox-libc/time.o \
	$(BUILD_DIR)/toybox-libc/system.o \
	$(BUILD_DIR)/toybox-libc/compat.o \
	$(BUILD_DIR)/toybox-libc/access.o \
	$(BUILD_DIR)/toybox-libc/stubs.o

VIX_RUNTIME_LIBC_OBJS := \
	$(BUILD_DIR)/toybox-libc/posix.o \
	$(BUILD_DIR)/toybox-libc/string.o \
	$(BUILD_DIR)/toybox-libc/stdio.o \
	$(BUILD_DIR)/toybox-libc/stdlib.o \
	$(BUILD_DIR)/toybox-libc/allocator.o

VIXC_GATE := $(BUILD_DIR)/vixc/capability.stamp
VIXC_CORE_OBJECT := $(BUILD_DIR)/vixc/vixc-core.o
VIXC_HELPER_OBJECT := $(BUILD_DIR)/vixc/freestanding-helper.o
VIXC_WRITER_OBJECT := $(BUILD_DIR)/vixc/elf-writer.o
VIXC_LINKER_OBJECT := $(BUILD_DIR)/vixc/elf-linker.o
VIXC_LANGUAGE_RUNTIME_OBJECT := $(BUILD_DIR)/vixc/language-runtime.o
VIXC_CORE_SOURCE := $(VIX_REPO)/src/aukos_main.vix
VIXC_VIX_SOURCES := $(shell find $(VIX_REPO)/src -type f -name '*.vix')
VIXC_HOST_TARGET := $(VIX_REPO)/build/vixc
ifeq ($(VIXC),$(VIXC_HOST_TARGET))
VIXC_HOST_DEP := $(VIXC_HOST_TARGET)
else
VIXC_HOST_DEP :=
endif

Q := @
quiet_cmd_cc = CC      $@
quiet_cmd_as = AS      $@
quiet_cmd_ld = LD      $@
quiet_cmd_iso = ISO     $@
quiet_cmd_iso_uefi = ISOUEFI $@
quiet_cmd_check = CHECK   $<
quiet_cmd_run = RUN     $(ISO)
quiet_cmd_run_uefi = RUNUEFI  $(ISO_UEFI)
quiet_cmd_run_debug = RUNDBG  $(ISO)
quiet_cmd_hostcc = HOSTCC  $@
quiet_cmd_usercc = USERCC  $@
quiet_cmd_userld = USERLD  $@
quiet_cmd_binobj = BINOBJ  $@
quiet_cmd_test = TEST    $<
quiet_cmd_clean = CLEAN   $(BUILD_DIR)
quiet_cmd_efi = EFI     $@
quiet_cmd_objcopy = OBJCOPY $@

CFLAGS := -target x86_64-unknown-none -std=c17 -ffreestanding -fno-stack-protector \
	-fno-pic -mno-red-zone -Wall -Wextra -Werror -Ikernel/include
USER_CFLAGS := -target x86_64-unknown-none -std=c17 -ffreestanding -fno-stack-protector \
	-fno-pic -mcmodel=large -mno-red-zone -Wall -Wextra -Werror -Iuser/include
ASFLAGS := -f elf64
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T arch/x86_64/linker.ld
LDFLAGS_UEFI := -nostdlib -static -z max-page-size=0x1000 -T arch/x86_64/linker_uefi.ld

EFI_CFLAGS := -target x86_64-unknown-none -std=c17 -ffreestanding -fno-stack-protector \
	-fpic -mno-red-zone -fshort-wchar -Wall -Wextra -Werror \
	-I$(EFI_INCLUDE_DIR) -I$(EFI_ARCH_INCLUDE_DIR) -DEFI_FUNCTION_WRAPPER
EFI_LDFLAGS := -nostdlib -static -T $(EFI_LDS) \
	$(EFI_CRT0) -L$(EFI_LIB_DIR) -lefi -lgnuefi

KERNEL_OBJS := \
	$(BUILD_DIR)/arch/x86_64/boot.o \
	$(BUILD_DIR)/arch/x86_64/interrupt_stubs.o \
	$(BUILD_DIR)/arch/x86_64/syscall_entry.o \
	$(BUILD_DIR)/arch/x86_64/user_entry.o \
	$(BUILD_DIR)/kernel/block.o \
	$(BUILD_DIR)/kernel/console.o \
	$(BUILD_DIR)/kernel/descriptor.o \
	$(BUILD_DIR)/kernel/elf.o \
	$(BUILD_DIR)/kernel/ext4.o \
	$(BUILD_DIR)/kernel/fat32.o \
	$(BUILD_DIR)/kernel/gdt.o \
	$(BUILD_DIR)/kernel/idt.o \
	$(BUILD_DIR)/kernel/keyboard.o \
	$(BUILD_DIR)/kernel/log.o \
	$(BUILD_DIR)/kernel/memory.o \
	$(BUILD_DIR)/kernel/multiboot2.o \
	$(BUILD_DIR)/kernel/net.o \
	$(BUILD_DIR)/kernel/net_packets.o \
	$(BUILD_DIR)/kernel/panic.o \
	$(BUILD_DIR)/kernel/pci.o \
	$(BUILD_DIR)/kernel/pipe.o \
	$(BUILD_DIR)/kernel/ramdisk.o \
	$(BUILD_DIR)/kernel/main.o \
	$(BUILD_DIR)/kernel/scheduler.o \
	$(BUILD_DIR)/kernel/serial.o \
	$(BUILD_DIR)/kernel/syscall.o \
	$(BUILD_DIR)/kernel/task.o \
	$(BUILD_DIR)/kernel/timer.o \
	$(BUILD_DIR)/kernel/user.o \
	$(BUILD_DIR)/kernel/vfs.o \
	$(BUILD_DIR)/kernel/tmpfs.o \
	$(BUILD_DIR)/kernel/udp_socket.o \
	$(BUILD_DIR)/kernel/vmm.o \
	$(BUILD_DIR)/kernel/vmm_stack.o \
	$(BUILD_DIR)/kernel/virtio_blk.o \
	$(BUILD_DIR)/kernel/virtio_net.o \
	$(BUILD_DIR)/user/hello.o \
	$(BUILD_DIR)/user/aush.o \
	$(BUILD_DIR)/user/write_test.o \
	$(BUILD_DIR)/user/signal_test.o \
	$(BUILD_DIR)/user/udp_test.o \
	$(BUILD_DIR)/user/dev_memory_test.o \
	$(BUILD_DIR)/user/pipe_test.o \
	$(BUILD_DIR)/user/process_env_test.o \
	$(BUILD_DIR)/user/process_stack_test.o \
	$(BUILD_DIR)/user/file_api_test.o \
	$(BUILD_DIR)/user/language_hello.o \
	$(BUILD_DIR)/user/language_runtime_test.o \
	$(USER_ED_BLOB) \
	$(USER_TOUCH_BLOB) \
	$(USER_VIXC_BLOB) \
	$(USER_VIXC_TEST_BLOB) \
	$(USER_PERSISTENCE_TEST_BLOB) \
	$(USER_NASM_TEST_BLOB) \
	$(USER_NASM_BLOB) \
	$(VIX_RUNTIME_BLOB) \
	$(BUILD_DIR)/user/toybox.o \
	$(VGA_FONT_OBJ)

KERNEL_UEFI_OBJS := \
	$(BUILD_DIR)/arch/x86_64/boot_uefi.o \
	$(BUILD_DIR)/arch/x86_64/interrupt_stubs.o \
	$(BUILD_DIR)/arch/x86_64/syscall_entry.o \
	$(BUILD_DIR)/arch/x86_64/user_entry.o \
	$(BUILD_DIR)/kernel/block.o \
	$(BUILD_DIR)/kernel/console.o \
	$(BUILD_DIR)/kernel/descriptor.o \
	$(BUILD_DIR)/kernel/elf.o \
	$(BUILD_DIR)/kernel/ext4.o \
	$(BUILD_DIR)/kernel/fat32.o \
	$(BUILD_DIR)/kernel/gdt.o \
	$(BUILD_DIR)/kernel/idt.o \
	$(BUILD_DIR)/kernel/keyboard.o \
	$(BUILD_DIR)/kernel/log.o \
	$(BUILD_DIR)/kernel/memory.o \
	$(BUILD_DIR)/kernel/net.o \
	$(BUILD_DIR)/kernel/net_packets.o \
	$(BUILD_DIR)/kernel/uefi_memory.o \
	$(BUILD_DIR)/kernel/panic.o \
	$(BUILD_DIR)/kernel/pci.o \
	$(BUILD_DIR)/kernel/pipe.o \
	$(BUILD_DIR)/kernel/ramdisk.o \
	$(BUILD_DIR)/kernel/main_uefi.o \
	$(BUILD_DIR)/kernel/scheduler.o \
	$(BUILD_DIR)/kernel/serial.o \
	$(BUILD_DIR)/kernel/syscall.o \
	$(BUILD_DIR)/kernel/task.o \
	$(BUILD_DIR)/kernel/timer.o \
	$(BUILD_DIR)/kernel/user.o \
	$(BUILD_DIR)/kernel/vfs.o \
	$(BUILD_DIR)/kernel/tmpfs.o \
	$(BUILD_DIR)/kernel/udp_socket.o \
	$(BUILD_DIR)/kernel/vmm.o \
	$(BUILD_DIR)/kernel/vmm_stack.o \
	$(BUILD_DIR)/kernel/virtio_blk.o \
	$(BUILD_DIR)/kernel/virtio_net.o \
	$(BUILD_DIR)/user/hello.o \
	$(BUILD_DIR)/user/aush.o \
	$(BUILD_DIR)/user/write_test.o \
	$(BUILD_DIR)/user/signal_test.o \
	$(BUILD_DIR)/user/udp_test.o \
	$(BUILD_DIR)/user/dev_memory_test.o \
	$(BUILD_DIR)/user/pipe_test.o \
	$(BUILD_DIR)/user/process_env_test.o \
	$(BUILD_DIR)/user/process_stack_test.o \
	$(BUILD_DIR)/user/file_api_test.o \
	$(BUILD_DIR)/user/language_hello.o \
	$(BUILD_DIR)/user/language_runtime_test.o \
	$(USER_ED_BLOB) \
	$(USER_TOUCH_BLOB) \
	$(USER_VIXC_BLOB) \
	$(USER_VIXC_TEST_BLOB) \
	$(USER_PERSISTENCE_TEST_BLOB) \
	$(USER_NASM_TEST_BLOB) \
	$(USER_NASM_BLOB) \
	$(VIX_RUNTIME_BLOB) \
	$(BUILD_DIR)/user/toybox.o \
	$(VGA_FONT_OBJ)

.PHONY: all iso iso-uefi run run-uefi run-debug smoke smoke-uefi check test toybox-host toybox-aukos-config toybox-aukos-port nasm-host nasm-aukos-port host-vixc clean

all: $(KERNEL) $(TOYBOX_AUKOS_BIN) $(VIRTIO_DISK) $(WORK_BASE_DISK) $(WORK_DISK)

iso: $(ISO)

iso-uefi: $(ISO_UEFI)

run: $(ISO) $(VIRTIO_DISK) $(WORK_DISK)
	$(Q)printf '%s\n' '$(quiet_cmd_run)'
	$(Q)$(QEMU) -drive file=$(ISO),format=raw,if=ide,media=cdrom,readonly=on \
		-drive file=$(VIRTIO_DISK),format=raw,if=none,id=virtio-disk \
		-device virtio-blk-pci,drive=virtio-disk,disable-modern=on \
		-drive file=$(WORK_DISK),format=raw,if=none,id=work-disk \
		-device virtio-blk-pci,drive=work-disk,disable-modern=on \
		$(QEMU_NET_ARGS) \
		-boot d -serial stdio -no-reboot -no-shutdown

run-uefi: $(ISO_UEFI) $(VIRTIO_DISK) $(WORK_DISK)
	$(Q)printf '%s\n' '$(quiet_cmd_run_uefi)'
	$(Q)$(QEMU) -drive file=$(ISO_UEFI),format=raw,if=ide,media=cdrom,readonly=on \
		-drive file=$(VIRTIO_DISK),format=raw,if=none,id=virtio-disk \
		-device virtio-blk-pci,drive=virtio-disk,disable-modern=on \
		-drive file=$(WORK_DISK),format=raw,if=none,id=work-disk \
		-device virtio-blk-pci,drive=work-disk,disable-modern=on \
		$(QEMU_NET_ARGS) \
		-bios $(OVMF_PATH) -serial stdio -display none -no-reboot -no-shutdown

run-debug: $(ISO) $(VIRTIO_DISK) $(WORK_DISK)
	$(Q)printf '%s\n' '$(quiet_cmd_run_debug)'
	$(Q)$(QEMU) -drive file=$(ISO),format=raw,if=ide,media=cdrom,readonly=on \
		-drive file=$(VIRTIO_DISK),format=raw,if=none,id=virtio-disk \
		-device virtio-blk-pci,drive=virtio-disk,disable-modern=on \
		-drive file=$(WORK_DISK),format=raw,if=none,id=work-disk \
		-device virtio-blk-pci,drive=work-disk,disable-modern=on \
		$(QEMU_NET_ARGS) \
		-boot d -serial stdio -display none -no-reboot -no-shutdown

smoke: $(ISO) $(VIRTIO_DISK) $(WORK_BASE_DISK) $(UDP_ECHO_SERVER) $(UDP_PCAP_CHECK) tools/check_qemu_log.sh tools/check_nasm_guest_artifacts.sh tools/run_qemu_smoke.sh
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)QEMU='$(QEMU)' E2FSCK='$(E2FSCK)' DEBUGFS='$(DEBUGFS)' \
		sh tools/run_qemu_smoke.sh bios $(ISO) $(VIRTIO_DISK) $(WORK_BASE_DISK) \
		$(BUILD_DIR)/bios-first-smoke.log $(BUILD_DIR)/bios-second-smoke.log \
		$(BUILD_DIR)/bios-udp.pcap $(UDP_ECHO_SERVER) $(OVMF_PATH) \
		$(BUILD_DIR)/bios-work-smoke.img
	$(Q)$(UDP_PCAP_CHECK) $(BUILD_DIR)/bios-udp-first.pcap
	$(Q)$(UDP_PCAP_CHECK) $(BUILD_DIR)/bios-udp.pcap

smoke-uefi: $(ISO_UEFI) $(VIRTIO_DISK) $(WORK_BASE_DISK) $(UDP_ECHO_SERVER) $(UDP_PCAP_CHECK) tools/check_qemu_log.sh tools/check_nasm_guest_artifacts.sh tools/run_qemu_smoke.sh
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)QEMU='$(QEMU)' E2FSCK='$(E2FSCK)' DEBUGFS='$(DEBUGFS)' \
		sh tools/run_qemu_smoke.sh uefi $(ISO_UEFI) $(VIRTIO_DISK) $(WORK_BASE_DISK) \
		$(BUILD_DIR)/uefi-first-smoke.log $(BUILD_DIR)/uefi-second-smoke.log \
		$(BUILD_DIR)/uefi-udp.pcap $(UDP_ECHO_SERVER) $(OVMF_PATH) \
		$(BUILD_DIR)/uefi-work-smoke.img
	$(Q)$(UDP_PCAP_CHECK) $(BUILD_DIR)/uefi-udp-first.pcap
	$(Q)$(UDP_PCAP_CHECK) $(BUILD_DIR)/uefi-udp.pcap

check: $(KERNEL)
	$(Q)printf '%s\n' '$(quiet_cmd_check)'
	$(Q)$(GRUB_FILE) --is-x86-multiboot2 $(KERNEL)

test: $(HOST_TEST_DIR)/ring_buffer_test \
	$(HOST_TEST_DIR)/pipe_test \
	$(HOST_TEST_DIR)/scheduler_test \
	$(HOST_TEST_DIR)/task_lifecycle_test \
	$(HOST_TEST_DIR)/vmm_stack_test \
	$(HOST_TEST_DIR)/path_test \
	$(HOST_TEST_DIR)/environment_test \
	$(HOST_TEST_DIR)/hardware_layout_test \
	$(HOST_TEST_DIR)/descriptor_test \
	$(HOST_TEST_DIR)/virtio_net_test \
	$(HOST_TEST_DIR)/udp_packet_test \
	$(HOST_TEST_DIR)/udp_socket_test \
	$(HOST_TEST_DIR)/shell_parse_test \
	$(HOST_TEST_DIR)/libgen_test \
	$(HOST_TEST_DIR)/stdio_format_test \
	$(HOST_TEST_DIR)/stdio_file_test \
	$(HOST_TEST_DIR)/string_test \
	$(HOST_TEST_DIR)/stdlib_abort_test \
	$(HOST_TEST_DIR)/ctype_test \
	$(HOST_TEST_DIR)/time_test \
	$(HOST_TEST_DIR)/allocator_test \
	$(HOST_TEST_DIR)/block_test \
	$(HOST_TEST_DIR)/ext4_write_test \
	$(HOST_TEST_DIR)/tmpfs_ownership_test \
	$(HOST_TEST_DIR)/mkstemp_test \
	$(HOST_TEST_DIR)/access_test \
	$(HOST_TEST_DIR)/elf_reader_test \
	$(VIRTIO_DISK) $(WORK_BASE_DISK) \
	$(USER_CHECK_DIR)/user_posix_compile.o \
	$(USER_CHECK_DIR)/libc/compat.o \
	$(USER_CHECK_DIR)/libc/access.o \
	$(USER_CHECK_DIR)/libc/ctype.o \
	$(USER_CHECK_DIR)/libc/environment.o \
	$(USER_CHECK_DIR)/libc/crt0.o \
	$(USER_CHECK_DIR)/libc/libgen.o \
	$(USER_CHECK_DIR)/libc/mkstemp.o \
	$(USER_CHECK_DIR)/libc/posix.o \
	$(USER_CHECK_DIR)/libc/stdio.o \
	$(USER_CHECK_DIR)/libc/stdlib.o \
	$(USER_CHECK_DIR)/libc/allocator.o \
	$(USER_CHECK_DIR)/libc/string.o \
	$(USER_CHECK_DIR)/libc/system.o \
	$(USER_CHECK_DIR)/libc/time.o
	$(Q)printf '%s\n' '$(quiet_cmd_test)'
	$(Q)$<
	$(Q)$(HOST_TEST_DIR)/pipe_test
	$(Q)$(HOST_TEST_DIR)/scheduler_test
	$(Q)$(HOST_TEST_DIR)/task_lifecycle_test
	$(Q)$(HOST_TEST_DIR)/vmm_stack_test
	$(Q)$(HOST_TEST_DIR)/path_test
	$(Q)$(HOST_TEST_DIR)/environment_test
	$(Q)$(HOST_TEST_DIR)/hardware_layout_test $(VIRTIO_DISK)
	$(Q)$(HOST_TEST_DIR)/descriptor_test
	$(Q)$(HOST_TEST_DIR)/virtio_net_test
	$(Q)$(HOST_TEST_DIR)/udp_packet_test
	$(Q)$(HOST_TEST_DIR)/udp_socket_test
	$(Q)$(HOST_TEST_DIR)/shell_parse_test
	$(Q)$(HOST_TEST_DIR)/libgen_test
	$(Q)$(HOST_TEST_DIR)/stdio_format_test
	$(Q)$(HOST_TEST_DIR)/stdio_file_test
	$(Q)$(HOST_TEST_DIR)/string_test
	$(Q)$(HOST_TEST_DIR)/stdlib_abort_test; abort_status=$$?; test $$abort_status -eq 134
	$(Q)$(HOST_TEST_DIR)/ctype_test
	$(Q)$(HOST_TEST_DIR)/time_test
	$(Q)$(HOST_TEST_DIR)/allocator_test
	$(Q)$(HOST_TEST_DIR)/block_test
	$(Q)$(HOST_TEST_DIR)/ext4_write_test $(WORK_BASE_DISK) $(BUILD_DIR)/ext4-write-result.img
	$(Q)$(E2FSCK) -fn $(WORK_BASE_DISK)
	$(Q)$(E2FSCK) -fn $(BUILD_DIR)/ext4-write-result.img
	$(Q)$(HOST_TEST_DIR)/tmpfs_ownership_test
	$(Q)$(HOST_TEST_DIR)/mkstemp_test
	$(Q)$(HOST_TEST_DIR)/access_test
	$(Q)$(HOST_TEST_DIR)/elf_reader_test

toybox-host: $(TOYBOX_HOST)

toybox-aukos-config: $(TOYBOX_AUKOS_CONFIG)

toybox-aukos-port: $(TOYBOX_AUKOS_BIN)

nasm-host: $(NASM_HOST)
	$(Q)$(MAKE) -C ports/nasm -f Makefile reference compact-test

nasm-aukos-port: $(NASM_AUKOS)
	$(Q)sh tools/check_nasm_artifact.sh $(NASM_AUKOS)

$(NASM_HOST): ports/nasm/Makefile ports/nasm/sources.mk $(NASM_UPSTREAM_FILES)
	$(Q)$(MAKE) -C ports/nasm -f Makefile host

$(NASM_AUKOS): ports/nasm/Makefile ports/nasm/sources.mk \
		ports/nasm/compact_insns.py ports/nasm/patches/compact-insns.patch \
		$(NASM_UPSTREAM_FILES) $(TOYBOX_LIBC_OBJS)
	$(Q)$(MAKE) -C ports/nasm -f Makefile aukos
	$(Q)sh tools/check_nasm_artifact.sh $@

$(TOYBOX_HOST): $(TOYBOX_DIR)/.config
	$(Q)$(MAKE) -C $(TOYBOX_DIR)

$(TOYBOX_DIR)/.config:
	$(Q)$(MAKE) -C $(TOYBOX_DIR) defconfig

$(TOYBOX_AUKOS_CONFIG): $(TOYBOX_DIR)/aukos_miniconfig
	$(Q)KCONFIG_ALLCONFIG=aukos_miniconfig $(MAKE) -C $(TOYBOX_DIR) KCONFIG_CONFIG=.config.aukos allnoconfig

$(TOYBOX_AUKOS_BIN): $(TOYBOX_AUKOS_CONFIG) $(TOYBOX_LIBC_OBJS) \
		$(TOYBOX_DIR)/main.c $(TOYBOX_DIR)/toys/posix/ls.c \
		user/include/unistd.h user/include/fcntl.h user/include/errno.h
	$(Q)mkdir -p $(dir $@)
	$(Q)rm -f $(TOYBOX_DIR)/generated/build.sh
	$(Q)$(MAKE) -C $(TOYBOX_DIR) \
		KCONFIG_CONFIG=.config.aukos \
		CC="$(CC)" \
		CFLAGS="$(TOYBOX_AUKOS_CFLAGS)" \
		LDOPTIMIZE="" \
		LDFLAGS="-nostdlib -static $(addprefix $(CURDIR)/,$(TOYBOX_LIBC_OBJS)) -Wl,-T,$(CURDIR)/user/linker.ld -Wl,-z,max-page-size=0x1000" \
		OUTNAME="$(CURDIR)/$@" \
		UNSTRIPPED="$(CURDIR)/$(BUILD_DIR)/toybox-aukos/unstripped" \
		GENDIR=generated \
		NOSTRIP=1 \
		toybox

$(BUILD_DIR)/toybox-libc/%.o: user/libc/%.c
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(TOYBOX_AUKOS_CFLAGS) -c $< -o $@

$(ISO): $(KERNEL) $(VIRTIO_DISK) boot/grub/grub.cfg
	@command -v $(XORRISO) >/dev/null || { echo "error: $(XORRISO) is required by $(GRUB_MKRESCUE)"; exit 1; }
	$(Q)printf '%s\n' '$(quiet_cmd_iso)'
	$(Q)rm -f $@
	$(Q)mkdir -p $(ISO_BIOS_DIR)/boot/grub
	$(Q)cp $(KERNEL) $(ISO_BIOS_DIR)/boot/aukos.elf
	$(Q)cp boot/grub/grub.cfg $(ISO_BIOS_DIR)/boot/grub/grub.cfg
	$(Q)$(GRUB_MKRESCUE) -o $@ $(ISO_BIOS_DIR)

$(ISO_UEFI): $(KERNEL_UEFI) $(BOOTLOADER_EFI) $(VIRTIO_DISK)
	@command -v $(XORRISO) >/dev/null || { echo "error: $(XORRISO) is required"; exit 1; }
	@command -v $(MMD) >/dev/null || { echo "error: $(MMD) from mtools is required"; exit 1; }
	@command -v $(MCOPY) >/dev/null || { echo "error: $(MCOPY) from mtools is required"; exit 1; }
	$(Q)printf '%s\n' '$(quiet_cmd_iso_uefi)'
	$(Q)rm -f $@
	$(Q)rm -rf $(ISO_UEFI_DIR)
	$(Q)mkdir -p $(ISO_UEFI_DIR)/EFI/BOOT
	$(Q)mkdir -p $(ISO_UEFI_DIR)/EFI/AUKOS
	$(Q)cp $(BOOTLOADER_EFI) $(ISO_UEFI_DIR)/EFI/BOOT/BOOTX64.EFI
	$(Q)cp $(KERNEL_UEFI) $(ISO_UEFI_DIR)/EFI/AUKOS/KERNEL.ELF
	$(Q)dd if=/dev/zero of=$(ISO_UEFI_DIR)/efi.img bs=1M count=4 2>/dev/null
	$(Q)$(MKFS_FAT) -F 12 $(ISO_UEFI_DIR)/efi.img 2>/dev/null
	$(Q)$(MMD) -i $(ISO_UEFI_DIR)/efi.img ::/EFI ::/EFI/BOOT ::/EFI/AUKOS
	$(Q)$(MCOPY) -i $(ISO_UEFI_DIR)/efi.img $(BOOTLOADER_EFI) ::/EFI/BOOT/BOOTX64.EFI
	$(Q)$(MCOPY) -i $(ISO_UEFI_DIR)/efi.img $(KERNEL_UEFI) ::/EFI/AUKOS/KERNEL.ELF
	$(Q)$(XORRISO) -as mkisofs -R -f \
		-e efi.img -no-emul-boot \
		-append_partition 2 0xef $(ISO_UEFI_DIR)/efi.img \
		-o $@ $(ISO_UEFI_DIR)
	$(Q)rm -f $(ISO_UEFI_DIR)/efi.img

$(KERNEL): $(KERNEL_OBJS) arch/x86_64/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_ld)'
	$(Q)$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

$(KERNEL_UEFI): $(KERNEL_UEFI_OBJS) arch/x86_64/linker_uefi.ld
	$(Q)printf '%s\n' '$(quiet_cmd_ld)'
	$(Q)$(LD) $(LDFLAGS_UEFI) -o $@ $(KERNEL_UEFI_OBJS)

$(BOOTLOADER_EFI): $(BOOTLOADER_ELF)
	$(Q)printf '%s\n' '$(quiet_cmd_objcopy)'
	$(Q)$(OBJCOPY) -j .text -j .sdata -j .data -j .rodata -j .dynamic \
		-j .dynsym -j .rel -j .rela -j '.rel.*' -j '.rela.*' -j .reloc \
		-O pei-x86-64 --subsystem=10 $< $@

$(BOOTLOADER_ELF): $(BUILD_DIR)/boot/uefi/bootloader.o $(EFI_LDS) $(EFI_CRT0)
	$(Q)printf '%s\n' '$(quiet_cmd_efi)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(LD) $(EFI_LDFLAGS) -o $@ $(BUILD_DIR)/boot/uefi/bootloader.o

$(BUILD_DIR)/boot/uefi/bootloader.o: boot/uefi/bootloader.c
	$(Q)printf '%s\n' '$(quiet_cmd_cc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(EFI_CFLAGS) -c $< -o $@

$(USER_DIR)/hello.elf: $(USER_DIR)/hello_entry.o user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ $(USER_DIR)/hello_entry.o

$(USER_DIR)/hello_entry.o: user/hello.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/aush.elf: $(USER_DIR)/aush_entry.o $(USER_AUSH_PARSE_OBJ) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ $(USER_DIR)/aush_entry.o $(USER_AUSH_PARSE_OBJ)

$(USER_DIR)/aush_entry.o: user/shell.c user/include/aukos/shell_parse.h
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_AUSH_PARSE_OBJ): user/shell_parse.c user/include/aukos/shell_parse.h
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/write_test.elf: $(USER_DIR)/write_test_entry.o user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ $(USER_DIR)/write_test_entry.o

$(USER_DIR)/write_test_entry.o: user/write_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_SIGNAL_TEST): $(USER_DIR)/signal_test_entry.o $(SIGNAL_TEST_LIBC_OBJ) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ $(USER_DIR)/signal_test_entry.o $(SIGNAL_TEST_LIBC_OBJ)

$(USER_DIR)/signal_test_entry.o: user/signal_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_UDP_TEST): $(USER_DIR)/udp_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/udp_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_DIR)/udp_test_entry.o: user/udp_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DEV_MEMORY_TEST): $(USER_DIR)/dev_memory_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/dev_memory_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_PIPE_TEST): $(USER_DIR)/pipe_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/pipe_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_PROCESS_ENV_TEST): $(USER_DIR)/process_env_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/process_env_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_PROCESS_STACK_TEST): $(USER_DIR)/process_stack_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/process_stack_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_FILE_API_TEST): $(USER_DIR)/file_api_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/file_api_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_VIX_HELLO): $(USER_VIX_HELLO_OBJ) $(USER_DIR)/entry.o \
		$(USER_DIR)/runtime.o $(VIX_RUNTIME_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/entry.o $(USER_DIR)/runtime.o \
		$(USER_VIX_HELLO_OBJ) $(VIX_RUNTIME_LIBC_OBJS)

$(USER_VIX_RUNTIME_TEST): $(USER_DIR)/runtime_test_entry.o \
		$(USER_DIR)/runtime.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/runtime_test_entry.o $(USER_DIR)/runtime.o \
		$(TOYBOX_LIBC_OBJS)

$(USER_ED): $(USER_ED_OBJ) $(USER_DIR)/entry.o \
		$(USER_DIR)/runtime.o $(VIXC_LANGUAGE_RUNTIME_OBJECT) \
		$(VIX_RUNTIME_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/entry.o $(USER_DIR)/runtime.o \
		$(USER_ED_OBJ) $(VIXC_LANGUAGE_RUNTIME_OBJECT) $(VIX_RUNTIME_LIBC_OBJS)

$(USER_TOUCH): $(USER_TOUCH_OBJ) $(USER_DIR)/entry.o \
		$(USER_DIR)/runtime.o $(VIXC_LANGUAGE_RUNTIME_OBJECT) \
		$(VIX_RUNTIME_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/entry.o $(USER_DIR)/runtime.o \
		$(USER_TOUCH_OBJ) $(VIXC_LANGUAGE_RUNTIME_OBJECT) $(VIX_RUNTIME_LIBC_OBJS)

host-vixc: $(VIXC_HOST_TARGET)

$(VIXC_HOST_TARGET): $(VIX_REPO)/makefile $(VIXC_VIX_SOURCES)
	$(Q)$(MAKE) -C $(VIX_REPO) build/vixc
	$(Q)touch $@

$(VIXC_GATE): tools/check_vixc_capability.sh $(VIXC_HOST_DEP)
	@command -v $(VIXC) >/dev/null || { echo "error: host Vix compiler not found: $(VIXC)"; exit 1; }
	$(Q)mkdir -p $(dir $@)
	$(Q)sh tools/check_vixc_capability.sh $(VIXC)
	$(Q)touch $@

$(VIXC_CORE_OBJECT): $(VIXC_CORE_SOURCE) $(VIXC_VIX_SOURCES) $(VIXC_GATE)
	$(Q)printf '%s\n' 'VIXC    $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)ulimit -s 65536 && $(VIXC) --target $(VIX_TARGET) -obj $< -o $@

$(VIXC_HELPER_OBJECT): $(VIX_REPO)/src/freestanding_helper.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(VIXC_WRITER_OBJECT): $(VIX_REPO)/src/elf_writer.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(VIXC_LINKER_OBJECT): $(VIX_REPO)/src/elf_linker.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(VIXC_LANGUAGE_RUNTIME_OBJECT): $(VIX_REPO)/src/runtime.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_VIXC): $(VIXC_CORE_OBJECT) $(VIXC_HELPER_OBJECT) \
		$(VIXC_WRITER_OBJECT) $(VIXC_LINKER_OBJECT) $(VIXC_LANGUAGE_RUNTIME_OBJECT) \
		$(TOYBOX_LIBC_OBJS) user/linker.ld tools/check_vixc_artifact.sh
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(VIXC_CORE_OBJECT) $(VIXC_HELPER_OBJECT) $(VIXC_WRITER_OBJECT) \
		$(VIXC_LINKER_OBJECT) \
		$(VIXC_LANGUAGE_RUNTIME_OBJECT) $(TOYBOX_LIBC_OBJS)
	$(Q)sh tools/check_vixc_artifact.sh $@ $(VIXC_CORE_OBJECT)

$(USER_VIXC_TEST): $(USER_DIR)/vixc_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/vixc_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_DIR)/vixc_test_entry.o: user/vixc_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_PERSISTENCE_TEST): $(USER_DIR)/persistence_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/persistence_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_NASM_TEST): $(USER_DIR)/nasm_test_entry.o $(TOYBOX_LIBC_OBJS) user/linker.ld
	$(Q)printf '%s\n' '$(quiet_cmd_userld)'
	$(Q)$(LD) -nostdlib -static -T user/linker.ld -o $@ \
		$(USER_DIR)/nasm_test_entry.o $(TOYBOX_LIBC_OBJS)

$(USER_DIR)/persistence_test_entry.o: user/persistence_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/nasm_test_entry.o: user/nasm_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(VIX_RUNTIME_OBJECT): $(USER_DIR)/entry.o $(USER_DIR)/runtime.o \
		$(VIXC_LANGUAGE_RUNTIME_OBJECT) $(VIX_RUNTIME_LIBC_OBJS)
	$(Q)printf '%s\n' 'USERLD  $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(LD) -r -o $@ $(USER_DIR)/entry.o $(USER_DIR)/runtime.o \
		$(VIXC_LANGUAGE_RUNTIME_OBJECT) $(VIX_RUNTIME_LIBC_OBJS)

$(USER_VIX_HELLO_OBJ): user/hello.vix $(VIXC_GATE)
	@command -v $(VIXC) >/dev/null || { echo "error: host Vix compiler not found: $(VIXC)"; exit 1; }
	$(Q)printf '%s\n' 'VIXC    $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)ulimit -s 65536 && $(VIXC) --target $(VIX_TARGET) -obj $< -o $@

$(USER_ED_OBJ): user/ed.vix $(VIXC_GATE)
	$(Q)printf '%s\n' 'VIXC    $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)ulimit -s 65536 && $(VIXC) --target $(VIX_TARGET) -obj $< -o $@

$(USER_TOUCH_OBJ): user/touch.vix $(VIXC_GATE)
	$(Q)printf '%s\n' 'VIXC    $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)ulimit -s 65536 && $(VIXC) --target $(VIX_TARGET) -obj $< -o $@

$(USER_DIR)/dev_memory_test_entry.o: user/dev_memory_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/pipe_test_entry.o: user/pipe_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/process_env_test_entry.o: user/process_env_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/process_stack_test_entry.o: user/process_stack_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/file_api_test_entry.o: user/file_api_test.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/entry.o: user/entry.c user/include/aukos/runtime.h
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/runtime.o: user/runtime.c user/include/aukos/runtime.h
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_DIR)/runtime_test_entry.o: user/runtime_test.c \
		user/include/aukos/runtime.h
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(SIGNAL_TEST_LIBC_OBJ): user/libc/system.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/hello.o: $(USER_HELLO)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/aush.o: $(USER_AUSH)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/write_test.o: $(USER_WRITE_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/signal_test.o: $(USER_SIGNAL_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/udp_test.o: $(USER_UDP_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/dev_memory_test.o: $(USER_DEV_MEMORY_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/pipe_test.o: $(USER_PIPE_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/process_env_test.o: $(USER_PROCESS_ENV_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/process_stack_test.o: $(USER_PROCESS_STACK_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/file_api_test.o: $(USER_FILE_API_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/language_hello.o: $(USER_VIX_HELLO)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/language_runtime_test.o: $(USER_VIX_RUNTIME_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_ED_BLOB): $(USER_ED)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_TOUCH_BLOB): $(USER_TOUCH)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_VIXC_BLOB): $(USER_VIXC)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_VIXC_TEST_BLOB): $(USER_VIXC_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_PERSISTENCE_TEST_BLOB): $(USER_PERSISTENCE_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_NASM_TEST_BLOB): $(USER_NASM_TEST)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(USER_NASM_BLOB): $(NASM_AUKOS) tools/check_nasm_artifact.sh
	$(Q)sh tools/check_nasm_artifact.sh $<
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(VIX_RUNTIME_BLOB): $(VIX_RUNTIME_OBJECT)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(BUILD_DIR)/user/toybox.o: $(TOYBOX_AUKOS_BIN)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(VGA_FONT_BIN): $(VGA_FONT_SOURCE)
	$(Q)printf '%s\n' 'FONT    $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)base64 -d $< | gzip -dc > $@
	$(Q)test "$$(wc -c < $@)" -eq 4096

$(VGA_FONT_OBJ): $(VGA_FONT_BIN)
	$(Q)printf '%s\n' '$(quiet_cmd_binobj)'
	$(Q)$(LD) -m elf_x86_64 -r -b binary -o $@ $<

$(VIRTIO_FIXTURE_GEN): tools/mkext4_fixture.c
	$(Q)printf '%s\n' 'HOSTCC  $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror $< -o $@

$(UDP_ECHO_SERVER): tools/udp_echo_server.c
	$(Q)printf '%s\n' 'HOSTCC  $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror $< -o $@

$(UDP_PCAP_CHECK): tools/check_udp_pcap.c kernel/net_packets.c \
		kernel/include/aukos/net_packets.h
	$(Q)printf '%s\n' 'HOSTCC  $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tools/check_udp_pcap.c kernel/net_packets.c -o $@

$(VIRTIO_DISK): $(VIRTIO_FIXTURE_GEN)
	$(Q)printf '%s\n' 'FIXTURE $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)$< $@

$(WORK_BASE_DISK): tools/mkwork_ext4.sh
	$(Q)printf '%s\n' 'WORKFS  $@'
	$(Q)mkdir -p $(dir $@)
	$(Q)tools/mkwork_ext4.sh $@

$(WORK_DISK): | $(WORK_BASE_DISK)
	$(Q)printf '%s\n' 'WORKIMG $@'
	$(Q)cp $(WORK_BASE_DISK) $@

$(BUILD_DIR)/%.o: %.c
	$(Q)printf '%s\n' '$(quiet_cmd_cc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	$(Q)printf '%s\n' '$(quiet_cmd_as)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(AS) $(ASFLAGS) $< -o $@

$(HOST_TEST_DIR)/%: tests/%.c
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include $< -o $@

$(HOST_TEST_DIR)/virtio_net_test: tests/virtio_net_test.c kernel/net_packets.c \
		kernel/include/aukos/net_packets.h kernel/include/aukos/virtio_net.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/virtio_net_test.c kernel/net_packets.c -o $@

$(HOST_TEST_DIR)/block_test: tests/block_test.c kernel/block.c \
		kernel/include/aukos/block.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/block_test.c kernel/block.c -o $@

$(HOST_TEST_DIR)/ext4_write_test: tests/ext4_write_test.c kernel/block.c \
		kernel/vfs.c kernel/ext4.c kernel/include/aukos/ext4.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		-DAUKOS_HOST_TEST tests/ext4_write_test.c kernel/block.c \
		kernel/vfs.c kernel/ext4.c -o $@

$(HOST_TEST_DIR)/udp_packet_test: tests/udp_packet_test.c kernel/net_packets.c \
		kernel/include/aukos/net_packets.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/udp_packet_test.c kernel/net_packets.c -o $@

$(HOST_TEST_DIR)/descriptor_test: tests/descriptor_test.c kernel/descriptor.c \
		kernel/pipe.c kernel/include/aukos/descriptor.h kernel/include/aukos/pipe.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/descriptor_test.c kernel/descriptor.c kernel/pipe.c -o $@

$(HOST_TEST_DIR)/pipe_test: tests/pipe_test.c kernel/pipe.c \
		kernel/include/aukos/pipe.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/pipe_test.c kernel/pipe.c -o $@

$(HOST_TEST_DIR)/scheduler_test: tests/scheduler_test.c kernel/scheduler.c \
		kernel/pipe.c kernel/include/aukos/scheduler.h kernel/include/aukos/pipe.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/scheduler_test.c kernel/scheduler.c kernel/pipe.c -o $@

$(HOST_TEST_DIR)/vmm_stack_test: tests/vmm_stack_test.c kernel/vmm_stack.c \
		kernel/include/aukos/vmm.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/vmm_stack_test.c kernel/vmm_stack.c -o $@

$(HOST_TEST_DIR)/task_lifecycle_test: tests/task_lifecycle_test.c kernel/task.c \
		kernel/include/aukos/task.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/task_lifecycle_test.c kernel/task.c -o $@

$(HOST_TEST_DIR)/path_test: tests/path_test.c kernel/vfs.c \
		kernel/include/aukos/vfs.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/path_test.c kernel/vfs.c -o $@

$(HOST_TEST_DIR)/tmpfs_ownership_test: tests/tmpfs_ownership_test.c \
		kernel/vfs.c kernel/tmpfs.c kernel/include/aukos/vfs.h \
		kernel/include/aukos/tmpfs.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		-DAUKOS_HOST_TEST tests/tmpfs_ownership_test.c kernel/vfs.c \
		kernel/tmpfs.c -o $@

$(HOST_TEST_DIR)/mkstemp_test: tests/mkstemp_test.c user/libc/mkstemp.c
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		-DAUKOS_MKSTEMP_TEST tests/mkstemp_test.c user/libc/mkstemp.c -o $@

$(HOST_TEST_DIR)/elf_reader_test: tests/elf_reader_test.c kernel/elf.c \
		kernel/vfs.c kernel/tmpfs.c $(USER_HELLO)
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		-DAUKOS_HOST_TEST tests/elf_reader_test.c kernel/elf.c kernel/vfs.c \
		kernel/tmpfs.c -o $@

$(HOST_TEST_DIR)/environment_test: tests/environment_test.c \
		user/libc/environment.c user/include/stdlib.h user/include/unistd.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		-DAUKOS_ENV_TEST_ALLOCATORS \
		tests/environment_test.c user/libc/environment.c -o $@

$(HOST_TEST_DIR)/udp_socket_test: tests/udp_socket_test.c kernel/udp_socket.c \
		kernel/include/aukos/udp_socket.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
		tests/udp_socket_test.c kernel/udp_socket.c -o $@

$(HOST_TEST_DIR)/shell_parse_test: tests/shell_parse_test.c user/shell_parse.c \
		user/include/aukos/shell_parse.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		tests/shell_parse_test.c user/shell_parse.c -o $@

$(HOST_TEST_DIR)/libgen_test: tests/libgen_test.c user/libc/libgen.c user/include/libgen.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		tests/libgen_test.c user/libc/libgen.c -o $@

$(HOST_TEST_DIR)/stdio_format_test: tests/stdio_format_test.c user/libc/stdio.c \
		user/include/stdio.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		-Derrno=aukos_test_errno \
		tests/stdio_format_test.c user/libc/stdio.c -o $@

$(HOST_TEST_DIR)/stdio_file_test: tests/stdio_file_test.c user/libc/stdio.c \
		user/include/stdio.h user/include/fcntl.h user/include/unistd.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		-Derrno=aukos_test_errno \
		-Dopen=aukos_stdio_open -Dread=aukos_stdio_read \
		-Dwrite=aukos_stdio_write -Dlseek=aukos_stdio_lseek \
		-Dclose=aukos_stdio_close -Dunlink=aukos_stdio_unlink \
		tests/stdio_file_test.c user/libc/stdio.c -o $@

$(HOST_TEST_DIR)/ctype_test: tests/ctype_test.c user/libc/ctype.c \
		user/include/ctype.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		tests/ctype_test.c user/libc/ctype.c -o $@

$(HOST_TEST_DIR)/string_test: tests/string_test.c user/libc/string.c \
		user/include/string.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -fno-builtin -Iuser/include \
		tests/string_test.c user/libc/string.c -o $@

$(HOST_TEST_DIR)/stdlib_abort_test: tests/stdlib_abort_test.c user/libc/stdlib.c \
		user/include/stdlib.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -fno-builtin \
		-ffunction-sections -Wl,--gc-sections -Iuser/include \
		-Dmalloc=aukos_abort_test_malloc -Dcalloc=aukos_abort_test_calloc \
		-Drealloc=aukos_abort_test_realloc -Dfree=aukos_abort_test_free \
		tests/stdlib_abort_test.c user/libc/stdlib.c -o $@

$(HOST_TEST_DIR)/access_test: tests/access_test.c user/libc/access.c \
		user/include/unistd.h user/include/sys/stat.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		-Dstat=aukos_access_stat \
		tests/access_test.c user/libc/access.c -o $@

$(HOST_TEST_DIR)/time_test: tests/time_test.c user/libc/time.c \
		user/include/time.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/include \
		tests/time_test.c user/libc/time.c -o $@

$(HOST_TEST_DIR)/allocator_test: tests/allocator_test.c user/libc/allocator.c \
		user/libc/allocator.h
	$(Q)printf '%s\n' '$(quiet_cmd_hostcc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -std=c17 -Wall -Wextra -Werror -Iuser/libc \
		tests/allocator_test.c user/libc/allocator.c -o $@

$(USER_CHECK_DIR)/%.o: tests/%.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

$(USER_CHECK_DIR)/libc/%.o: user/libc/%.c
	$(Q)printf '%s\n' '$(quiet_cmd_usercc)'
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(USER_CFLAGS) -c $< -o $@

clean:
	$(Q)printf '%s\n' '$(quiet_cmd_clean)'
	$(Q)rm -rf $(BUILD_DIR)
