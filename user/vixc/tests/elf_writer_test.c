#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int allocation_calls;
static int allocation_fail_at = -1;
static int force_short_write;

static void *test_malloc(size_t size) {
  if (allocation_fail_at >= 0 && allocation_calls++ == allocation_fail_at)
    return NULL;
  return malloc(size);
}

static void *test_realloc(void *pointer, size_t size) {
  if (allocation_fail_at >= 0 && allocation_calls++ == allocation_fail_at)
    return NULL;
  return realloc(pointer, size);
}

static size_t test_fwrite(const void *data, size_t size, size_t count,
                          FILE *file) {
  if (force_short_write && count) return 0;
  return fwrite(data, size, count, file);
}

#define VIX_ELF_MALLOC test_malloc
#define VIX_ELF_REALLOC test_realloc
#define VIX_ELF_FWRITE test_fwrite
#include "../src/elf_writer.c"

static int failures;
static char output_path[128];

#define CHECK(CONDITION)                                                       \
  do {                                                                         \
    if (!(CONDITION)) {                                                        \
      fprintf(stderr, "elf_writer_test:%d: %s\n", __LINE__, #CONDITION);     \
      failures++;                                                              \
    }                                                                          \
  } while (0)

static void reset_injection(void) {
  allocation_calls = 0;
  allocation_fail_at = -1;
  force_short_write = 0;
  remove(output_path);
}

static void release_assembler(struct assembler *assembler) {
  free(assembler->text.data);
  free(assembler->rodata.data);
}

static int contains_bytes(const unsigned char *data, size_t size,
                          const unsigned char *needle, size_t needle_size) {
  if (needle_size > size) return 0;
  for (size_t offset = 0; offset <= size - needle_size; offset++)
    if (!memcmp(data + offset, needle, needle_size)) return 1;
  return 0;
}

static void check_public_failure(const char *assembly) {
  reset_injection();
  CHECK(vix_elf_write_object(assembly, output_path) != 0);
  CHECK(access(output_path, F_OK) != 0);
}

static void test_valid_object(void) {
  static const char assembly[] =
      "default rel\n"
      "section .text\n"
      "extern external_fn\n"
      "global local_fn\n"
      "local_fn:\n"
      "push rbp\n"
      "mov rbp, rsp\n"
      "sub rsp, 32\n"
      "mov r8, 8\n"
      "mov r9d, 9\n"
      "mov r10, rsp\n"
      "mov r11, qword [rbp-8]\n"
      "mov qword [rsp+8], r8\n"
      "mov qword [rbp-16], 17\n"
      "mov al, 0\n"
      "call external_fn\n"
      "call local_fn\n"
      "mov rsp, rbp\n"
      "pop rbp\n"
      "ret\n"
      "section .data\n"
      "message: db 111, 107, 0\n"
      "number: dq 9.0\n";
  reset_injection();
  CHECK(vix_elf_write_object(assembly, output_path) == 0);
  CHECK(access(output_path, F_OK) == 0);
  remove(output_path);

  struct assembler assembler = {0};
  CHECK(assemble(&assembler, assembly) == 0);
  CHECK(assembler.relocation_count == 2);
  for (int index = 0; index < assembler.relocation_count; index++)
    CHECK(assembler.relocations[index].type == R_X86_64_PLT32);
  int local = find_symbol(&assembler, "local_fn", 0);
  CHECK(local >= 0 && assembler.symbols[local].defined);
  static const unsigned char r8_immediate[] =
      {0x49, 0xb8, 8, 0, 0, 0, 0, 0, 0, 0};
  static const unsigned char r9d_immediate[] = {0x41, 0xb9, 9, 0, 0, 0};
  static const unsigned char r10_rsp[] = {0x4c, 0x8b, 0xd4};
  static const unsigned char r11_rbp[] = {0x4c, 0x8b, 0x5d, 0xf8};
  static const unsigned char rsp_sib[] = {0x4c, 0x89, 0x44, 0x24, 0x08};
  static const unsigned char memory_immediate[] =
      {0x48, 0xc7, 0x45, 0xf0, 17, 0, 0, 0};
  CHECK(contains_bytes(assembler.text.data, assembler.text.size, r8_immediate,
                       sizeof(r8_immediate)));
  CHECK(contains_bytes(assembler.text.data, assembler.text.size, r9d_immediate,
                       sizeof(r9d_immediate)));
  CHECK(contains_bytes(assembler.text.data, assembler.text.size, r10_rsp,
                       sizeof(r10_rsp)));
  CHECK(contains_bytes(assembler.text.data, assembler.text.size, r11_rbp,
                       sizeof(r11_rbp)));
  CHECK(contains_bytes(assembler.text.data, assembler.text.size, rsp_sib,
                       sizeof(rsp_sib)));
  CHECK(contains_bytes(assembler.text.data, assembler.text.size,
                       memory_immediate, sizeof(memory_immediate)));
  release_assembler(&assembler);
}

static void test_parser_failures(void) {
  check_public_failure("section .text\nglobal f\nf:\nret\nf:\nret\n");
  check_public_failure("section .text\nglobal f\nf:\njmp missing\n");
  check_public_failure("section .text\nglobal f\nf:\ncall missing\nret\n");
  check_public_failure("section .text\nglobal f\nf:\nnop\n");
  check_public_failure("section .data\nbad: db 1,\n");
  check_public_failure("section .data\nbad: dw 1\n");
  check_public_failure(
      "section .text\nglobal f\nf:\nmov qword [rbp-8], 0x80000000\nret\n");
}

static void test_integer_boundaries(void) {
  int64_t value = 0;
  CHECK(parse_integer("9223372036854775807", &value) == 0 &&
        value == INT64_MAX);
  CHECK(parse_integer("-9223372036854775808", &value) == 0 &&
        value == INT64_MIN);
  CHECK(parse_integer("9223372036854775808", &value) != 0);
  CHECK(parse_integer("-9223372036854775809", &value) != 0);
  double floating = 0.0;
  CHECK(parse_double("9.0", &floating) == 0 && floating == 9.0);
  CHECK(parse_double("1.2.3", &floating) != 0);
}

static void test_internal_overflow_checks(void) {
  struct byte_buffer buffer = {0};
  CHECK(buffer_reserve(&buffer, VIX_ELF_MAX_OUTPUT + 1u) != 0);

  struct assembler branch = {0};
  branch.text.data = malloc(4);
  branch.text.size = 4;
  branch.text.capacity = 4;
  branch.symbol_count = 1;
  branch.symbols[0].defined = 1;
  branch.symbols[0].section = SEC_TEXT;
  branch.symbols[0].value = (uint64_t)INT32_MAX + 1;
  branch.fixup_count = 1;
  branch.fixups[0] = (struct branch_fixup){0, 0};
  CHECK(resolve_fixups(&branch) != 0);
  release_assembler(&branch);

  struct assembler relocation = {0};
  CHECK(buffer_u32(&relocation.text, 0) == 0);
  relocation.symbol_count = 1;
  strcpy(relocation.symbols[0].name, "target");
  relocation.symbols[0].defined = 1;
  relocation.symbols[0].global = 1;
  relocation.symbols[0].section = SEC_TEXT;
  relocation.relocation_count = 1;
  relocation.relocations[0] =
      (struct object_relocation){0, 0, 99, -4};
  reset_injection();
  CHECK(write_object(&relocation, output_path) != 0);
  CHECK(access(output_path, F_OK) != 0);
  release_assembler(&relocation);
}

static void test_io_and_allocator_failures(void) {
  static const char assembly[] =
      "section .text\nglobal f\nf:\nmov eax, 1\nret\n";
  reset_injection();
  force_short_write = 1;
  CHECK(vix_elf_write_object(assembly, output_path) != 0);
  CHECK(access(output_path, F_OK) != 0);

  for (int fail_at = 0; fail_at < 3; fail_at++) {
    reset_injection();
    allocation_fail_at = fail_at;
    CHECK(vix_elf_write_object(assembly, output_path) != 0);
    CHECK(access(output_path, F_OK) != 0);
  }
  reset_injection();
}

int main(void) {
  snprintf(output_path, sizeof(output_path), "/tmp/vix-elf-writer-%ld.o",
           (long)getpid());
  test_valid_object();
  test_parser_failures();
  test_integer_boundaries();
  test_internal_overflow_checks();
  test_io_and_allocator_failures();
  remove(output_path);
  if (failures) return 1;
  puts("elf_writer_test: PASS");
  return 0;
}
