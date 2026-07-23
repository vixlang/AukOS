# VGA font asset

`vga8x16.bin.gz.b64` is a base64-encoded gzip stream containing the 4096-byte
`alt-8x16` console font distributed by the Linux `kbd` project. It contains 256
glyphs with 16 rows per glyph and is licensed under GPL-2.0-or-later.

The source asset is stored in compressed text form so it can be reviewed and
reproduced without making the AukOS build depend on a host-installed console
font. The Makefile decodes it into `build/vga8x16.bin` before linking it into
the kernel.
