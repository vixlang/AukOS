# ToyBox on AukOS

## Build boundary

AukOS uses `toybox/aukos_miniconfig` with ToyBox `allnoconfig` to keep the
ported command set explicit:

```sh
make toybox-aukos-config
make toybox-aukos-port
```

The port build compiles ToyBox source files with the AukOS freestanding
headers, then links the generated objects with the AukOS libc and user linker
script. Source compilation and the final AukOS link both propagate failures to
the top-level Makefile. The build does not depend on an ignored upstream link
failure or stale objects.

## Command dispatch

`aush` parses each input line once into at most 16 NUL-terminated argv entries.
It supports space and tab separators, single and double quotes, backslash
escaping, and empty quoted arguments. Unquoted `|`, `&`, `;`, `<`, and `>` are
rejected because their shell semantics are not implemented.

Only shell-control commands remain built into aush: `help`, `clear`, `fs`,
`cd`, `signal_test`, and `exit`. Every other command passes the parsed entries
unchanged through the real AukOS `fork`, `execve`, and `waitpid` path to
`/bin/toybox`. This makes plain `cat`, `echo`, `ls`, and the other enabled
commands ToyBox applet invocations by default. The explicit
`toybox APPLET ARG...` multiplexer form remains available, and `toybox` with no
arguments still lists the compiled applets.

`waitpid` returns the child's raw AukOS exit code; this is used to verify that
`true` returns zero and `false` returns nonzero.

The 12 enabled and BIOS/UEFI-tested applets are:

```text
basename cat dirname echo false head ls printf pwd true uname wc
```

The BIOS and UEFI QEMU regressions exercise the applets as plain commands,
including arguments, quoting, options, strings, the root working directory,
and files from the read-only `/ext4` fixture. In particular, they check
`head -n 1`, the real 23-byte `wc -c` count, `printf '%s\n'`, and the final
`[toybox_test] default command dispatch PASS` marker.

`cd` stays in aush because ToyBox marks its implementation `TOYFLAG_NOFORK`:
running `/bin/toybox cd` as a child cannot change the parent shell directory.
The builtin follows ToyBox command behavior for `cd [DIR]`, `cd -`, defaulting
to `$HOME`, and accepting `-L`/`-P`. It updates `PWD` and `OLDPWD` for later
ToyBox children. BIOS and UEFI smoke call the ToyBox `pwd` applet after each
change to prove the directory persists in aush.

## Current limits

This is a deliberately small ToyBox port, not a claim that every upstream
command works. The current filesystem surface is tmpfs plus read-only ext4,
the process and libc ABIs remain incomplete, and aush manages the working
directory with its `cd` builtin. File-mutating, process-table, and network
applets are not enabled.

`aush` is not a POSIX shell. It has no expansion, command substitution, glob,
pipeline, redirection, background job, job control, history, completion, PATH
search, dynamic linking, or ToyBox symlink farm.
