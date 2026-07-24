# Native NASM on AukOS

## Source and license

AukOS vendors the official Netwide Assembler source from
`https://github.com/netwide-assembler/nasm.git` in `user/nasm`. The port is
based on NASM 3.02, upstream commit
`b2734a91bde0d7ddaf8e6953c9821922d0a8675e`, described by upstream as
`nasm-3.02-49-gb2734a91b`. NASM is BSD-2-Clause; the full copyright and license
remain in `user/nasm/LICENSE`, and the bundled zlib license remains in
`user/nasm/zlib/LICENSE`.

This is the upstream parser, preprocessor, assembler, instruction database,
and ELF backend. It is not a source-pattern translator or a replacement
assembler. AukOS carries a small port configuration, source list, build glue,
and patch in `ports/nasm`.

## Build split

Run:

```sh
make nasm-host
make nasm-aukos-port
```

`make nasm-host` copies the vendored snapshot to `build/nasm-host/source`,
configures it out of tree, and builds the reference executable at
`build/nasm-host/nasm`. Upstream host tools, including Perl generators, create
the instruction, register, token, directive, warning, standard-macro, and
version tables in the host object tree.

The AukOS cross build consumes those generated C/header files and compiles the
official sources for `x86_64-unknown-none` with freestanding, static,
no-red-zone flags. No generator runs in the guest. The final
`build/nasm-aukos/nasm.elf` uses AukOS libc and `user/linker.ld`; its audited
blob is embedded as the same `/bin/nasm` in the BIOS and UEFI kernels.

The generated instruction table is represented more compactly for the 1 MiB
guest file boundary. `ports/nasm/test_compact_insns.py` compares it against the
upstream host table and proves that all 12,320 official instruction templates
are preserved. Source and version changes under `user/nasm` trigger the
incremental port rules. An unchanged normal `make` does not reconfigure NASM
or regenerate its tables.

## CLI and output commit

The supported native interface is:

```sh
/bin/nasm --version
/bin/nasm --help
/bin/nasm -f elf64 /work/hello.asm -o /work/hello.o
```

Absolute paths, cwd-relative paths, and upstream's `--` option terminator are
supported. Only `elf64` output is enabled. Missing or duplicate arguments,
missing input, syntax errors, unsupported formats, and an input/output
same-path request fail with a diagnostic and nonzero status.

Output uses a same-directory `mkstemp` followed by complete write, `fflush`,
`fsync`, close, and `rename`. A failure removes the temporary and preserves an
existing destination. Same-directory commit is required because cross-mount
rename is not supported. Guest tests also verify that temporary names do not
accumulate and repeat assemble/unlink eight times. Eight rounds exercise fd,
process, heap, inode, dirent, and block reuse without putting the much larger
upstream encoding matrix into every QEMU boot.

The normal NASM paths rely on AukOS implementations of file access and stat,
stdio read/write/seek/tell/error reporting, `rt`/`wt` modes, time conversion
and bounded formatting, character classification, integer constant and
format macros, `strpbrk`, `mkstemp`, `fsync`, and rename. `abort` terminates
with the conventional status 134 because AukOS does not expose a complete
host-style process signal runtime to this port.

## Object and linker contract

The smoke source exports `main`, uses RIP-relative data addressing, and calls
the runtime symbol `aukos_vix_write_str`. Both the host reference and guest
object must be little-endian x86-64 `ET_REL` files containing `.text`,
`.rodata`, `.symtab`, `.strtab`, and `.rela.text`. The required symbol and
relocation shape is:

```text
GLOBAL main
GLOBAL UND aukos_vix_write_str
R_X86_64_PC32  .rodata - 4
R_X86_64_PLT32 aukos_vix_write_str - 4
```

The existing restricted linker accepts only ordinary allocatable text,
read-only data, data, and bss sections, with `R_X86_64_64`,
`R_X86_64_PC32`, or `R_X86_64_PLT32`. `/bin/vixc --link hello.o -o hello`
adds `/lib/vix/runtime.o`, emits a static `ET_EXEC`, and retains the existing
ELF bounds and W^X checks.

## Native and persistence smoke

Interactive `make run` and `make run-uefi` enter aush without the long test
suite. `make smoke` and `make smoke-uefi` each make a private work image and
perform two boots. The first boot runs the real guest pipeline:

```text
/work/nasm-test/hello.asm
  -> /bin/nasm -f elf64
  -> /work/nasm-test/hello.o
  -> /bin/vixc --link with /lib/vix/runtime.o
  -> /work/nasm-test/hello
  -> execve
```

It fsyncs the source, object, executable, and completion marker. The second
boot first executes the persisted executable, then assembles, links, and runs
a second output to prove namespace and storage reuse. After QEMU exits, the
runner extracts the guest object and executable, audits them with host
`readelf`, checks both UDP captures, and runs `e2fsck -fn` on the final private
work image.

## Limits

The native port does not provide `ndisasm`, RDOFF, non-ELF output formats,
debug formats, documentation/install tooling, or the full upstream test
matrix. It is not a general dynamic linker, PIE/shared-object toolchain, LLD,
`ar`, `make`, or C/C++ compiler. Each source, object, NASM executable, and
linked guest executable must fit AukOS's existing 1 MiB per-file boundary;
the restricted linker also retains its section, symbol, and relocation
limits. Writable ext4 has no JBD2 journal, so successful `fsync` and clean
shutdown persistence is tested, but sudden-power-loss metadata atomicity is
not claimed.

## Clean acceptance

The 2026-07-23 clean acceptance built the pinned host and AukOS ports from a
fresh top-level `build` directory. The host executable was 4,643,256 bytes,
its reference object was 960 bytes, and the static AukOS executable was
959,696 bytes. The compact-table audit retained 12,320 of 12,320 templates.

BIOS and UEFI each passed both persistence boots. Every one of the four logs
contained 16 Vixc generated executions and 16 direct-driver executions. The
first-boot logs contained two NASM-generated execution markers; the
second-boot logs contained three, including execution of the first boot's
persisted ELF before the second object/executable pair was produced. Native
CLI, output preservation, temporary cleanup, and eight-round reuse gates all
passed.

Both firmware paths extracted the same 944-byte `ET_REL` object and
37,384-byte static `ET_EXEC`. Host `readelf` confirmed the required sections,
symbols, `R_X86_64_PC32` and `R_X86_64_PLT32` relocations, and RX/R/RW load
segments. All four UDP captures passed their packet contract. Final BIOS and
UEFI private work images each completed all five read-only `e2fsck -fn`
passes with 56/512 files and 129/8184 blocks in use.
