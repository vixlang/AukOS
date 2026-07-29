# AukOS

AukOS (A Unix-like Kernel) is an experimental x86_64 operating system built
with C17, Vix, and assembly. It currently boots through BIOS/GRUB and UEFI,
enters ring 3, runs static ELF programs, and provides a small shell, ToyBox
utilities, tmpfs, restricted ext4 read/write support, virtio block devices,
and basic virtio networking.

## Build

Host tools, source locations, EFI paths, and QEMU settings live in
`config.mk`. Its defaults target Arch Linux. For a local setup:

```sh
cp config.mk config.local.mk
make CONFIG_MK=config.local.mk
make CONFIG_MK=config.local.mk iso-uefi
```

Run host checks with `make test check`. QEMU smoke tests are available as
`make smoke` and `make smoke-uefi`.

The native assembler is built with `make nasm-host nasm-aukos-port`. Inside
AukOS, `/bin/nasm -f elf64 input.asm -o output.o` produces a standard ELF64
relocatable object for the restricted `/bin/vixc --link` path. See
`docs/nasm.md` for the build split, object contract, smoke workflow, and
current limits.

ToyBox, the AukOS-specific Vixc snapshot, and NASM are vendored as ordinary
source directories. They are not submodules. See `docs/third-party.md` for
their upstream repositories, pinned commits, and licenses.

## Source layout

The native source tree is organized by subsystem:

```text
arch/x86_64/          x86_64 boot code, linker scripts, and arch-specific C
kernel/core/          kernel entry points and core services
kernel/mm/            memory and virtual-memory management
kernel/fs/            VFS, filesystems, block layer, and ramdisk support
kernel/net/            Ethernet, IP, UDP, and TCP networking
kernel/drivers/       PCI, serial, input, VirtIO, and e1000 drivers
user/apps/            user programs and Vix applications
user/runtime/         user-space runtime and startup code
user/tests/           user-space test programs
tests/                host-side kernel and libc tests
tools/                host utilities and smoke-test scripts
```
