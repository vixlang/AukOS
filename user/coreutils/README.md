# AukOS Vix Coreutils

Every `*.vix` file in this directory is built as an independent static AukOS
program and embedded into the initial `/bin` tmpfs directory using the source
file stem as the command name.

For example, `echo.vix` builds to `/bin/echo`.

Command filenames must contain only ASCII letters, digits, and underscores so
their names also form valid linker symbols.
