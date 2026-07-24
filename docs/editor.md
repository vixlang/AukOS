# Editing Vix programs on AukOS

AukOS includes `/bin/ed`, a small full-screen modal editor written in Vix,
and `/bin/touch`. Both commands operate on the writable tmpfs mounted at `/`.

Create and edit a source file:

```text
[root@aukos/]$ touch /work/hello.vix
[root@aukos/]$ ed /work/hello.vix
```

`ed` opens in Normal mode. Press `i`, enter the program, then press `Esc`.
Type `:w` followed by Enter to save, and `:q` followed by Enter to quit.

A minimal AukOS program uses the normal Vix entry point:

```vix
fn main(): i32
{
    print("Hello from Vix on AukOS")
    return 0
}
```

Programs that need process arguments may instead declare
`fn main(argc: i32, argv: &string, envp: &string): i32`.

Compile, link, and run it inside AukOS:

```text
[root@aukos/]$ vixc -obj /work/hello.vix -o /work/hello.o
[root@aukos/]$ vixc --link /work/hello.o -o /work/hello
[root@aukos/]$ /work/hello
Hello from Vix on AukOS
```

Normal mode supports:

```text
h j k l    move the cursor
0 $        move to the beginning or end of the line
i a        enter Insert mode at or after the cursor
o O        create a line below or above and enter Insert mode
x          delete the character under the cursor
dd         delete the current line
gg G       jump to the first or last line
:          enter Command mode
```

Command mode supports:

```text
:w         save the current file
:w FILE    save under another name
:q         quit if there are no unsaved changes
:q!        discard changes and quit
:wq        save and quit
:e FILE    load another file if the buffer is unmodified
:help      show a compact command reminder
```

When stdin is a pipe or redirected file, `ed` uses its line-oriented script
mode. This keeps commands such as `printf 'a\ntext\n.\nw file\nq\n' | ed`
available for build scripts and tests.

`touch` accepts one or more paths and creates any missing regular files
without truncating existing files. AukOS does not yet expose a timestamp
update syscall, so touching an existing file preserves it but does not update
its access or modification time.
