# AukOS user memory ABI

## Address layout

The initial x86_64 user layout is fixed and intentionally small:

| Range | Owner |
| --- | --- |
| `0x40000000..0x50000000` | Static `ET_EXEC` ELF image |
| `0x50000000..0x60000000` | `brk` heap |
| `0x60000000..0x7fef6000` | Anonymous `mmap` regions |
| `0x7fef6000..0x7fef7000` | Signal-stack guard page |
| `0x7fef7000..0x7feff000` | Eight-page signal stack |
| `0x7feff000..0x7ff00000` | Exec-stack guard page |
| `0x7ff00000..0x80000000` | 1 MiB exec-stack reservation |

Each address space has 32 fixed VM region slots. ELF, heap, anonymous mappings,
the exec stack, and signal stacks all have explicit ownership. Slot exhaustion,
range overlap, overflow, unaligned addresses, and reserved-range access fail
deterministically.

The exec stack initially maps only its top eight pages. User-mode, non-present,
non-instruction page faults within the reservation and at most 64 KiB from RSP
allocate one zeroed page on demand. Protection faults, kernel faults, the guard,
and addresses beyond the 1 MiB cap are rejected. Fork copies only mapped stack
pages while retaining the full reservation; teardown walks the sparse page
tables. The exec builder still supports at most 64 argv entries, 64 envp
entries, 4096 bytes per string, and 32 KiB for strings and pointer tables
together. User entry preserves the AukOS SysV x86_64 stack alignment.

The `SYSCALL` instruction does not switch stacks in hardware, so the entry stub
immediately saves the user RSP and switches to a dedicated 16 KiB kernel syscall
stack. Blocking and exec transitions continue to use the independent resume
stack before changing or destroying an address space.

## Syscalls

The memory calls use the Linux x86_64 syscall numbers:

| Number | Call | Initial contract |
| --- | --- | --- |
| 9 | `mmap` | Exact `MAP_PRIVATE | MAP_ANONYMOUS`, `fd == -1`, `offset == 0` |
| 10 | `mprotect` | Full anonymous region only |
| 11 | `munmap` | Full anonymous region only; partial and double unmap fail |
| 12 | `brk` | Returns the resulting/current break; failure leaves it unchanged |

Lengths are rounded up to 4096-byte pages. A null `mmap` address selects the
first available address; a non-null address is an exact, page-aligned request.
`PROT_READ`, `PROT_WRITE`, and `PROT_EXEC` are supported. NX is enabled and
writable plus executable mappings are rejected.

The libc `sbrk` wrapper uses `brk`; `malloc` grows this page-backed heap in
bounded chunks. Allocation metadata records block size, and free blocks are
split, coalesced, and reused.

## Lifecycle

Anonymous and heap pages are zero-filled. `fork` eagerly copies only registered
VM regions and preserves actual PTE permissions. `execve` builds a complete new
space before commit; failure retains the old image. Successful exec switches to
the new CR3 and a kernel resume stack before freeing the old pages and page
tables. Exit closes descriptors immediately so pipe readers observe EOF, then
destroys the address space after switching CR3. PID, PPID, and the raw AukOS exit
status remain as zombie metadata until one successful `waitpid` reap clears the
process slot.

Each process owns a bounded cwd initialized to `/`. `fork` copies it and
`execve` preserves it. Relative path syscalls resolve against that cwd through
one normalizer for `.`, `..`, and repeated slashes; traversal above root stays
at root and overflow fails without changing process state.

The physical allocator keeps returned pages in an intrusive free list, so VM
teardown supports repeated mapping/fork/exit cycles instead of consuming the
fixed page pool.

## Regression coverage

`tests/vmm_stack_test.c` freezes stack layout and page-fault policy. Kernel
startup exercises initial mapping rollback, on-demand growth, sparse fork clone,
growth allocation failure, teardown, and physical-page accounting.
`tests/task_lifecycle_test.c` freezes immediate descriptor release, zombie
metadata retention, one-shot reap, and process-slot reuse. The ring3
`/bin/process_stack_test` covers both wait orderings, pipe EOF before wait,
duplicate wait, `waitpid(-1)`, `WNOHANG`, 40 reuse rounds, deep stack growth,
fork after growth, and guard/cap/protection faults.

`tests/allocator_test.c` covers alignment, zero size, overflow, split,
coalescing, reuse, calloc zeroing, realloc growth/shrink, and allocation-failure
preservation. Kernel startup checks map/protect/destroy page accounting.
`/bin/dev_memory_test` covers more than 1 MiB of heap, anonymous mappings,
invalid requests, W^X, full-unmap policy, address/slot reuse, and 16 complete
fork/isolation/exit/reap stress iterations. It also fills all 28 mmap slots left
after the ELF, heap, and stacks, verifies deterministic exhaustion, and releases
them for reuse. BIOS and UEFI smoke both require its
`[dev_memory_test] PASS` result.

`/bin/process_env_test` covers 40 argv entries, repeated 512-byte strings,
cross-page argv/envp construction, environment mutation, PATH lookup, cwd
inheritance, and relative file access across fork/exec.
