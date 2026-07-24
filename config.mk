# Host build environment. Override any value on the make command line or edit
# this file for the local toolchain layout.

ARCH := x86_64
BUILD_DIR := build

CC := clang
AS := nasm
LD := ld.lld
OBJCOPY := objcopy
QEMU := qemu-system-x86_64
GRUB_FILE := grub-file
GRUB_MKRESCUE := grub-mkrescue
XORRISO := xorriso
MKFS_FAT := mkfs.fat
MMD := mmd
MCOPY := mcopy
E2FSCK := e2fsck
DEBUGFS := debugfs

TOYBOX_DIR := toybox
VIX_REPO := user/vixc
VIXC := $(VIX_REPO)/build/vixc
NASM_REPO := user/nasm
VIX_TARGET := x86_64-unknown-none

EFI_INCLUDE_DIR := /usr/include/efi
EFI_ARCH_INCLUDE_DIR := $(EFI_INCLUDE_DIR)/x86_64
EFI_LIB_DIR := /usr/lib
EFI_LDS := $(EFI_LIB_DIR)/elf_x86_64_efi.lds
EFI_CRT0 := $(EFI_LIB_DIR)/crt0-efi-x86_64.o
OVMF_PATH := /usr/share/edk2/x64/OVMF.4m.fd
