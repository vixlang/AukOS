# Vix host-cross ABI

The initial Vix target is `x86_64-unknown-none`. Host `vixc` emits a standard
ELF64 relocatable object with the SysV x86_64 calling convention; `ld.lld`
links it as a static `ET_EXEC` using `user/linker.ld`. The resulting image uses
the address layout in `user-memory.md` and contains no interpreter, dynamic
library, TLS, or implicit host libc dependency.

The C entry shim owns `_start(uint64_t argc, char **argv, char **envp)` and
calls a normal Vix function. C and Vix exchange only documented scalar,
pointer, and return-value types under SysV. AukOS syscalls remain in the C
runtime shim so their register ABI has one stable implementation.

`/bin/vix_hello` is built as:

```text
user/coreutils/vix_hello.vix
  -> vixc --target x86_64-unknown-none -obj
  -> build/user/coreutils/vix_hello.vix.o
  +  user/runtime/entry.c
  +  user/runtime/runtime.c
  -> static build/user/coreutils/vix_hello.elf
```

The Vix function checks `argc`, indexes `argv`, walks `envp`, calls C string
and write shims, and propagates distinct nonzero failures. BIOS and UEFI smoke
execute `/bin/vix_hello --vix-smoke` through normal VFS, ELF, scheduler, and
descriptor paths and require `[vix_hello] PASS`.
