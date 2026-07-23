# AukOS file API, tmpfs, and writable ext4

## Fixed limits and ownership

The VFS owns a pool of 64 `vfs_file` open objects. Allocation happens before
`O_CREAT`, so pool exhaustion cannot leave a new namespace entry behind. A
process descriptor points to a reference-counted descriptor object; `dup`,
`dup2`, and `fork` therefore share one `vfs_file` and its current offset. The
last descriptor release closes and returns the VFS open slot.

tmpfs has 64 reusable inode slots, 64 children per directory, a 1 MiB maximum
regular-file size, and at most 256 independently allocated 4 KiB pages per
file. A node records its namespace link and open count. `unlink` removes the
name immediately, but pages and the inode slot remain valid until the last open
file closes. Rename replacement uses the same deferred reclaim rule. All
metadata is cleared before a reclaimed slot is reused.

Files embedded into `/bin` have static linker-owned backing. Reads and exec are
allowed, while writes and truncation fail; their addresses are never passed to
the physical page allocator. Files created under `/tmp` own sparse page arrays.
Missing pages read as zero, new pages are zeroed, truncate returns pages to the
PMM, and failed allocation rolls back before exposing a new size. `/work` is a
separate writable ext4 mount backed by `virtio1`.

## File semantics

- `O_CREAT | O_EXCL` checks the existing namespace entry before truncation.
- `O_RDONLY` rejects write and `O_WRONLY` rejects read. Directories cannot be
  sought or truncated.
- `O_APPEND` samples the current inode size for every write, including writes
  through distinct open objects.
- Regular-file seek may move beyond EOF. A later write creates a zero-filled
  hole, with offset and size overflow checked before mutation.
- `truncate` and `ftruncate` support writable dynamic tmpfs regular files from
  zero through 1 MiB. Shrink releases complete trailing pages and zeroes the
  retained tail; grow is sparse. `ftruncate` never changes the file offset.
- `rename` supports regular files within one writable tmpfs or ext4 mount. Same-path is
  a no-op, a missing source leaves the target untouched, and replacement is
  atomic after validation. Directories and cross-mount operations fail.
- tmpfs `fsync` is a validated no-op because the filesystem is volatile. On
  writable ext4 it flushes data, writes inode/related metadata, and issues a
  final virtio block flush. Read-only ext4 accepts a validated no-op; pipes,
  sockets, standard
  descriptors, directories, closed descriptors, and invalid descriptors fail.
- libc `mkstemp` requires six trailing `X` bytes, uses mode 0600 with
  `O_CREAT | O_EXCL | O_RDWR`, retries at most 64 collisions, and restores the
  six `X` bytes if all attempts fail.

The calls use Linux x86_64 syscall numbers 74 (`fsync`), 76 (`truncate`), 77
(`ftruncate`), and 82 (`rename`). Path calls use the process cwd normalizer.
Kernel failures return the common syscall failure sentinel and libc exposes
`-1`; invalid `mkstemp` templates use `EINVAL` and exhausted retries use
`EEXIST`.

## Writable ext4 subset and failure boundary

The writable mount accepts only the deterministic 4 KiB, single-group
`filetype extent` image. It supports regular files, one-block directories,
depth-zero inline extents, create/open/close/read/write, append and holes,
truncate, mkdir/readdir/stat, unlink with deferred reclaim, same-mount rename
replacement, and fsync. File size remains capped at 1 MiB. Hard links, symlink
mutation, xattrs, ACLs, quotas, indexed directories, deep extent mutation,
journal replay, and online resize are rejected.

Allocation updates inode/block bitmaps and superblock/group free counts. New
blocks are zeroed before an extent exposes them. `fsync` orders data flush,
inode metadata, then a final `VIRTIO_BLK_T_FLUSH`. Unlink removes the namespace
entry before reclaim; open unlinked/replaced inodes survive until last close.
An unrecoverable write or flush error marks only that mount errored/read-only.

There is no JBD2 journal. After a successful sequence and `fsync`, stopping
QEMU and booting the same image preserves data and executable files. AukOS
does not claim atomic metadata or rename recovery for sudden power loss. An
interruption before fsync may leak storage; offline `e2fsck` detects this, but
smoke never uses `e2fsck -y` to manufacture a passing image.

## Executable reader

The ELF loader accepts a bounded `read_at(context, offset, buffer, size)`
reader. It reads the ELF header, each program header, and each `PT_LOAD` page
without requiring contiguous backing. Every addition and table/segment bound
is checked. `execve` opens a regular file through VFS, loads through
`vfs_read_at`, closes the open slot on every path, and commits only after the
new image, stacks, argv, and envp are complete.

`/bin/file_api_test` copies its own static ELF in 4 KiB chunks to
`/work/generated.elf`, calls `fsync`, then forks and executes the copied inode.
Only that generated child prints `[generated_exec_test] PASS` after checking
argv, envp, and cwd. The parent waits for exit status zero before printing
`[file_api_test] PASS`.

## Regression coverage

Interactive `make run` and `make run-uefi` boots enter aush without running the
exhaustive startup suite. The smoke runners create `/work/.aukos-smoke` only in
their private work images; aush uses it to enable the full file, compiler,
editor, ToyBox, process, and network regressions.

`tmpfs_ownership_test` covers node/open-slot exhaustion, deterministic reuse,
deferred reclaim, access flags, append/hole/truncate, rename replacement, 128
namespace cycles, 256 KiB page grow/shrink/reuse, and page-allocation rollback.
`mkstemp_test` injects collisions, retry exhaustion, and invalid templates.
`elf_reader_test` loads the same ELF through memory and page-backed tmpfs
readers and rejects an injected short/error reader. BIOS and UEFI smoke require
the two generated-file markers plus every earlier process, pipe, shell, ext4,
memory, ToyBox, and UDP regression.

`block_test` adds 16 checks for aligned/unaligned RMW, zero length, last-byte,
capacity/overflow, two-device isolation, and injected read/write/flush failure.
`ext4_write_test` adds 29 checks for mutation and 128 reuse rounds, flush
propagation/read-only fallback, and corrupt feature, geometry, bitmap, extent,
and dirent rejection. Its result and the base image pass `e2fsck -fn`.
