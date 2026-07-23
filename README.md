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

ToyBox, the AukOS-specific Vixc snapshot, and NASM are vendored as ordinary
source directories. They are not submodules. See `docs/third-party.md` for
their upstream repositories, pinned commits, and licenses.
