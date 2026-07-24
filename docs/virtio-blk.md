# virtio-blk support

AukOS currently supports two QEMU transitional virtio-blk PCI devices
(`1af4:1001`) through the legacy I/O-port register layout. The driver uses one
legacy split virtqueue per device, one request per device at a time, and
bounded polling for completion. Each device owns its I/O base, capacity,
queue memory, rings, request header/status, and 4 KiB DMA bounce buffer. PCI
scan order names them `virtio0` and `virtio1`; failures remain isolated.

The implementation deliberately does not claim support for modern PCI
capabilities, packed queues, interrupts, MSI/MSI-X, multiple queues, or
concurrent requests. The QEMU run targets use `disable-modern=on` so that the
tested transport matches this scope.

Requests carry at most eight 512-byte sectors. Completion validates the used
index, descriptor id and length, device status byte, range, and timeout. The
driver negotiates `VIRTIO_BLK_F_FLUSH` and implements `VIRTIO_BLK_T_FLUSH`.
`block_flush` fails when the feature is absent, so writable ext4 cannot report
a no-op as durable storage.

`build/virtio-ext4.img` is generated deterministically by
`tools/mkext4_fixture.c`. Its first 32 KiB contain the ext4 fixture and its last
512-byte sector is reserved for the driver's write/read/restore self-test.
`/ext4` has no RAM-disk fallback: a normal QEMU boot must register `virtio0`
before ext4 can mount and expose `hello.txt`.

`build/aukos-work-base.img` is a deterministic 32 MiB device image containing
an 8184-block, single-group ext4 filesystem. It has a fixed UUID/time, 4 KiB
blocks, 512 inodes, and only `filetype extent` features. Eight trailing device
blocks lie outside the filesystem for a read/write/restore probe. `virtio1`
mounts it at `/work` only after feature, geometry, bitmap/count, read/write,
and flush checks pass.

`build/aukos-work.img` is the manual state disk. Make copies it from the base
only when absent, preserving it across `make run` restarts. Smoke uses separate
BIOS and UEFI copies, boots each copy twice, retains the final images, and runs
`e2fsck -fn` without repair.
