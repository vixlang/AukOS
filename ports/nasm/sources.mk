# Official NASM sources needed by the assembler and the ELF64 output backend.
NASM_GENERATED_SOURCES := \
	asm/directbl.c asm/pptok.c asm/tokhash.c macros/macros.c \
	x86/iflag.c x86/insnsa.c x86/insnsb.c x86/insnsn.c \
	x86/regs.c x86/regflags.c x86/regvals.c

NASM_SOURCES := \
	asm/nasm.c \
	asm/error.c asm/floats.c asm/directiv.c asm/pragma.c \
	asm/assemble.c asm/labels.c asm/parser.c asm/preproc.c asm/quote.c \
	asm/listing.c asm/eval.c asm/exprlib.c asm/exprdump.c asm/stdscan.c \
	asm/getbool.c asm/strfunc.c asm/segalloc.c asm/rdstrnum.c asm/srcfile.c \
	asm/directbl.c asm/pptok.c asm/tokhash.c asm/uncompress.c asm/warnings.c \
	macros/macros.c \
	output/outform.c output/outlib.c output/nulldbg.c output/nullout.c \
	output/outelf.c \
	stdlib/strlcpy.c stdlib/strnlen.c \
	nasmlib/ver.c nasmlib/alloc.c nasmlib/asprintf.c \
	nasmlib/crc32b.c nasmlib/crc64.c nasmlib/md5c.c \
	nasmlib/string.c nasmlib/nctype.c nasmlib/file.c nasmlib/fileio.c \
	nasmlib/mmap.c nasmlib/realpath.c nasmlib/path.c nasmlib/ilog2.c \
	nasmlib/numstr.c nasmlib/rlimit.c nasmlib/zerobuf.c nasmlib/bsi.c \
	nasmlib/rbtree.c nasmlib/hashtbl.c nasmlib/raa.c nasmlib/saa.c \
	nasmlib/strlist.c nasmlib/perfhash.c nasmlib/badenum.c nasmlib/readnum.c \
	common/common.c common/errstubs.c common/files.c \
	x86/insnsa.c x86/insnsb.c x86/insnsn.c x86/regs.c x86/regvals.c \
	x86/regflags.c x86/iflag.c \
	zlib/adler32.c zlib/crc32.c zlib/infback.c zlib/inffast.c \
	zlib/inflate.c zlib/inftrees.c zlib/zutil.c

