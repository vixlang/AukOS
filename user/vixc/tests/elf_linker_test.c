#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int allocation_calls;
static int allocation_fail_at = -1;
static int force_short_read;
static int force_short_write;

static void *test_malloc(size_t size) {
  if (allocation_fail_at >= 0 && allocation_calls++ == allocation_fail_at)
    return NULL;
  return malloc(size);
}
static void *test_calloc(size_t count, size_t size) {
  if (allocation_fail_at >= 0 && allocation_calls++ == allocation_fail_at)
    return NULL;
  return calloc(count, size);
}
static size_t test_fread(void *data, size_t size, size_t count, FILE *file) {
  if (force_short_read && count) return 0;
  return fread(data, size, count, file);
}
static size_t test_fwrite(const void *data, size_t size, size_t count,
                          FILE *file) {
  if (force_short_write && count) return 0;
  return fwrite(data, size, count, file);
}

#define VIX_LINK_MALLOC test_malloc
#define VIX_LINK_CALLOC test_calloc
#define VIX_LINK_FREAD test_fread
#define VIX_LINK_FWRITE test_fwrite
#include "../src/elf_linker.c"

static int failures;
#define CHECK(CONDITION)                                                       \
  do {                                                                         \
    if (!(CONDITION)) {                                                        \
      fprintf(stderr, "elf_linker_test:%d: %s\n", __LINE__, #CONDITION);     \
      failures++;                                                              \
    }                                                                          \
  } while (0)

static void reset_injection(void) {
  allocation_calls = 0;
  allocation_fail_at = -1;
  force_short_read = 0;
  force_short_write = 0;
}

static int load_inputs(struct link_context *context, const char *app,
                       const char *runtime) {
  memset(context, 0, sizeof(*context));
  for (int category = 0; category < OUTPUT_CATEGORY_COUNT; category++)
    context->category_alignment[category] = 1;
  return read_input_file(app, &context->inputs[0]) ||
         read_input_file(runtime, &context->inputs[1]);
}

static int prepare_link(struct link_context *context, const char *app,
                        const char *runtime) {
  return load_inputs(context, app, runtime) || validate_input(context, 0) ||
         validate_input(context, 1) || add_global_definitions(context, 0) ||
         add_global_definitions(context, 1) || assign_addresses(context) ||
         check_undefined_symbols(context) || allocate_and_copy_sections(context);
}

static struct elf64_rela *first_relocation(struct link_input *input) {
  for (int index = 0; index < input->header->shnum; index++) {
    struct elf64_section *section =
        (struct elf64_section *)&input->sections[index];
    if (section->type == SHT_RELA && section->size >= sizeof(struct elf64_rela))
      return (struct elf64_rela *)(input->bytes + section->offset);
  }
  return NULL;
}

static void test_table_failures(const char *app, const char *runtime) {
  struct link_context context;
  reset_injection();
  CHECK(load_inputs(&context, app, runtime) == 0);
  CHECK(validate_input(&context, 0) == 0);
  CHECK(validate_input(&context, 1) == 0);
  CHECK(add_global_definitions(&context, 0) == 0);
  CHECK(add_global_definitions(&context, 0) != 0);
  release_context(&context);

  CHECK(load_inputs(&context, app, runtime) == 0);
  CHECK(validate_input(&context, 0) == 0);
  CHECK(validate_input(&context, 1) == 0);
  CHECK(add_global_definitions(&context, 1) == 0);
  CHECK(check_undefined_symbols(&context) != 0);
  release_context(&context);

  CHECK(load_inputs(&context, app, runtime) == 0);
  struct elf64_header *header = (struct elf64_header *)context.inputs[0].bytes;
  header->shnum = VIX_LINK_MAX_SECTIONS + 1;
  CHECK(validate_input(&context, 0) != 0);
  release_context(&context);

  CHECK(load_inputs(&context, app, runtime) == 0);
  header = (struct elf64_header *)context.inputs[0].bytes;
  struct elf64_section *sections = (struct elf64_section *)(
      context.inputs[0].bytes + header->shoff);
  int first = -1, second = -1;
  for (int index = 1; index < header->shnum; index++) {
    if (sections[index].type != SHT_NOBITS && sections[index].size) {
      if (first < 0) first = index;
      else { second = index; break; }
    }
  }
  CHECK(first >= 0 && second >= 0);
  sections[second].offset = sections[first].offset;
  CHECK(validate_input(&context, 0) != 0);
  release_context(&context);
}

static void test_relocation_failures(const char *app, const char *runtime) {
  struct link_context context;
  CHECK(prepare_link(&context, app, runtime) == 0);
  struct elf64_rela *relocation = first_relocation(&context.inputs[0]);
  CHECK(relocation != NULL);
  if (relocation) {
    relocation->info = (relocation->info & ~UINT64_C(0xffffffff)) | 99;
    CHECK(apply_relocations(&context, 0) != 0);
  }
  release_context(&context);

  CHECK(prepare_link(&context, app, runtime) == 0);
  relocation = first_relocation(&context.inputs[0]);
  CHECK(relocation != NULL);
  if (relocation) {
    relocation->addend = INT64_MAX;
    CHECK(apply_relocations(&context, 0) != 0);
  }
  release_context(&context);
}

static void test_io_failures(const char *app, const char *runtime,
                             const char *output) {
  remove(output);
  reset_injection();
  force_short_read = 1;
  CHECK(vix_elf_link_static(app, runtime, output) != 0);
  CHECK(access(output, F_OK) != 0);

  reset_injection();
  force_short_write = 1;
  CHECK(vix_elf_link_static(app, runtime, output) != 0);
  CHECK(access(output, F_OK) != 0);

  reset_injection();
  allocation_fail_at = 0;
  CHECK(vix_elf_link_static(app, runtime, output) != 0);
  CHECK(access(output, F_OK) != 0);
  reset_injection();
}

int main(int argc, char **argv) {
  if (argc != 4) return 2;
  test_table_failures(argv[1], argv[2]);
  test_relocation_failures(argv[1], argv[2]);
  test_io_failures(argv[1], argv[2], argv[3]);
  CHECK(vix_elf_link_static(argv[1], argv[2], argv[3]) == 0);
  CHECK(access(argv[3], F_OK) == 0);
  if (failures) return 1;
  puts("elf_linker_test: PASS");
  return 0;
}
