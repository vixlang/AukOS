# NASM for AukOS

This port uses the official Netwide Assembler sources vendored in
`user/nasm`.  The snapshot is NASM 3.02 at upstream commit
`b2734a91bde0d7ddaf8e6953c9821922d0a8675e` (`nasm-3.02-49-gb2734a91b`),
obtained from <https://github.com/netwide-assembler/nasm.git>.  NASM is
distributed under the 2-clause BSD license in `user/nasm/LICENSE`; the
vendored zlib copy retains its license in `user/nasm/zlib/LICENSE`.

`make nasm-host` creates an out-of-tree upstream build.  Perl runs on the
host to generate the instruction, register, token, directive, warning,
standard-macro, and version tables.  `make nasm-aukos-port` then compiles the
official parser, preprocessor, assembler, and ELF backend with the AukOS
headers and libc.  No generator is run in AukOS.

The AukOS binary intentionally exposes only the `elf64` output format.  It is
freestanding, statically linked with `user/linker.ld`, and has no host CRT,
libc, dynamic loader, filesystem path, or command runner dependency.

