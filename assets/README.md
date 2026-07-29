# Console font asset

`terminus_vga8x20.bin.gz.b64` is a base64-encoded gzip stream containing a
5120-byte Terminus console font. It contains 256 glyphs with 20 rows per glyph
and is licensed under the SIL Open Font License, Version 1.1.

The source asset is derived from Terminus Font 4.49.1 `ter-i20n.psf`, the IBM
CP437 10x20 Linux console font. Because VGA text mode stores one byte per glyph
row, each 10-pixel row is horizontally condensed into 8 pixels before being
stored here. The Makefile decodes the asset into `build/terminus_vga8x20.bin`
before linking it into the kernel. The framebuffer console draws it directly;
the VGA fallback uses the same 8-pixel-wide rows.
