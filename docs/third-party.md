# Vendored source snapshots

AukOS keeps the source trees below as ordinary files in the main repository.
They are not Git submodules, so a clone of AukOS is sufficient to reproduce the
current build.

## ToyBox

- Upstream: `https://github.com/landley/toybox`
- Base commit: `b04b38efa74d4461fc7890c572f3ffecf46af6f8`
- License: 0BSD-style license in `toybox/LICENSE`
- AukOS carries local changes to the entry point, `ls`, and the minimal config.

## Vixc

- Upstream project: `https://github.com/vixlang/Vix-lang.git`
- Base commit: `361cc9a325d05479cbadb3c5ff4160883abb50cf`
- AukOS carries a substantial local bootstrap backend, ELF writer/linker, driver,
  runtime, tests, and generated seed changes under `user/vixc`.
- This customized snapshot is maintained as part of AukOS and is not submitted
  to the Vix-lang repository by the AukOS build workflow.

## NASM

- Upstream: `https://github.com/netwide-assembler/nasm.git`
- Snapshot commit: `b2734a91bde0d7ddaf8e6953c9821922d0a8675e`
- Description at import: `nasm-3.02-49-gb2734a91b`
- License: NASM license in `user/nasm/LICENSE`

The nested `.git` directories used in the original development workspace are
local metadata and are not part of the AukOS repository.
