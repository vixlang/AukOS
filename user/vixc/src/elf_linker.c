#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef INT32_MIN
#define INT32_MIN (-INT32_MAX - 1)
#endif

#ifndef VIX_LINK_MALLOC
#define VIX_LINK_MALLOC malloc
#endif
#ifndef VIX_LINK_CALLOC
#define VIX_LINK_CALLOC calloc
#endif
#ifndef VIX_LINK_FREE
#define VIX_LINK_FREE free
#endif
#ifndef VIX_LINK_FREAD
#define VIX_LINK_FREAD fread
#endif
#ifndef VIX_LINK_FWRITE
#define VIX_LINK_FWRITE fwrite
#endif

#define VIX_LINK_MAX_INPUT_SIZE (1024u * 1024u)
#define VIX_LINK_MAX_OUTPUT_SIZE (1024u * 1024u)
#define VIX_LINK_MAX_SECTIONS 64
#define VIX_LINK_MAX_SYMBOLS 512
#define VIX_LINK_MAX_GLOBALS 512
#define VIX_LINK_PAGE_SIZE 4096u
#define VIX_LINK_BASE_ADDRESS 0x40000000ULL

enum {
  ET_REL = 1,
  ET_EXEC = 2,
  EM_X86_64 = 62,
  PT_LOAD = 1,
  PF_X = 1,
  PF_W = 2,
  PF_R = 4,
  SHT_NULL = 0,
  SHT_PROGBITS = 1,
  SHT_SYMTAB = 2,
  SHT_STRTAB = 3,
  SHT_RELA = 4,
  SHT_NOBITS = 8,
  SHF_WRITE = 1,
  SHF_ALLOC = 2,
  SHF_EXECINSTR = 4,
  SHF_TLS = 0x400,
  SHN_UNDEF = 0,
  SHN_ABS = 0xfff1,
  STB_LOCAL = 0,
  STB_GLOBAL = 1,
  R_X86_64_64 = 1,
  R_X86_64_PC32 = 2,
  R_X86_64_PLT32 = 4,
  OUTPUT_TEXT = 0,
  OUTPUT_RODATA = 1,
  OUTPUT_DATA = 2,
  OUTPUT_BSS = 3,
  OUTPUT_CATEGORY_COUNT = 4,
  OUTPUT_SECTION_COUNT = 6,
};

struct elf64_header {
  unsigned char ident[16];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
};

struct elf64_program {
  uint32_t type;
  uint32_t flags;
  uint64_t offset;
  uint64_t virtual_address;
  uint64_t physical_address;
  uint64_t file_size;
  uint64_t memory_size;
  uint64_t alignment;
};

struct elf64_section {
  uint32_t name;
  uint32_t type;
  uint64_t flags;
  uint64_t address;
  uint64_t offset;
  uint64_t size;
  uint32_t link;
  uint32_t info;
  uint64_t alignment;
  uint64_t entry_size;
};

struct elf64_symbol {
  uint32_t name;
  unsigned char info;
  unsigned char other;
  uint16_t shndx;
  uint64_t value;
  uint64_t size;
};

struct elf64_rela {
  uint64_t offset;
  uint64_t info;
  int64_t addend;
};

struct section_mapping {
  int category;
  uint64_t offset;
};

struct link_input {
  unsigned char *bytes;
  size_t size;
  const struct elf64_header *header;
  const struct elf64_section *sections;
  const struct elf64_symbol *symbols;
  size_t symbol_count;
  const char *strings;
  size_t string_size;
  int symtab_index;
  struct section_mapping mappings[VIX_LINK_MAX_SECTIONS];
};

struct global_symbol {
  const char *name;
  int input;
  size_t symbol;
};

struct link_context {
  struct link_input inputs[2];
  struct global_symbol globals[VIX_LINK_MAX_GLOBALS];
  size_t global_count;
  uint64_t category_size[OUTPUT_CATEGORY_COUNT];
  uint64_t category_alignment[OUTPUT_CATEGORY_COUNT];
  uint64_t category_address[OUTPUT_CATEGORY_COUNT];
  unsigned char *category_bytes[OUTPUT_CATEGORY_COUNT];
};

_Static_assert(sizeof(struct elf64_header) == 64, "ELF64 header size");
_Static_assert(sizeof(struct elf64_program) == 56, "ELF64 program size");
_Static_assert(sizeof(struct elf64_section) == 64, "ELF64 section size");
_Static_assert(sizeof(struct elf64_symbol) == 24, "ELF64 symbol size");
_Static_assert(sizeof(struct elf64_rela) == 24, "ELF64 rela size");

static int range_valid(uint64_t offset, uint64_t size, uint64_t limit) {
  return offset <= limit && size <= limit - offset;
}

static int power_of_two(uint64_t value) {
  return value && !(value & (value - 1));
}

static int align_value(uint64_t value, uint64_t alignment, uint64_t *result) {
  if (!alignment) alignment = 1;
  if (!power_of_two(alignment) || alignment > VIX_LINK_PAGE_SIZE ||
      value > UINT64_MAX - (alignment - 1))
    return -1;
  *result = (value + alignment - 1) & ~(alignment - 1);
  return 0;
}

static int string_valid(const char *strings, size_t size, uint32_t offset) {
  return offset < size && memchr(strings + offset, '\0', size - offset) != NULL;
}

static int read_input_file(const char *path, struct link_input *input) {
  FILE *file = fopen(path, "rb");
  if (!file) return -1;
  if (fseek(file, 0, 2) || ftell(file) < 0) {
    fclose(file);
    return -1;
  }
  long length = ftell(file);
  if (length <= 0 || (unsigned long)length > VIX_LINK_MAX_INPUT_SIZE ||
      fseek(file, 0, 0)) {
    fclose(file);
    return -1;
  }
  input->bytes = VIX_LINK_MALLOC((size_t)length);
  if (!input->bytes) {
    fclose(file);
    return -1;
  }
  input->size = (size_t)length;
  size_t offset = 0;
  while (offset < input->size) {
    size_t count =
        VIX_LINK_FREAD(input->bytes + offset, 1, input->size - offset, file);
    if (!count) {
      fclose(file);
      return -1;
    }
    offset += count;
  }
  if (fclose(file)) return -1;
  return 0;
}

static int classify_section(const struct elf64_section *section) {
  if (!(section->flags & SHF_ALLOC)) return -1;
  if ((section->flags & SHF_TLS) ||
      ((section->flags & SHF_WRITE) && (section->flags & SHF_EXECINSTR)))
    return -2;
  if (section->flags & SHF_EXECINSTR)
    return section->type == SHT_PROGBITS ? OUTPUT_TEXT : -2;
  if (section->flags & SHF_WRITE) {
    if (section->type == SHT_NOBITS) return OUTPUT_BSS;
    return section->type == SHT_PROGBITS ? OUTPUT_DATA : -2;
  }
  return section->type == SHT_PROGBITS ? OUTPUT_RODATA : -2;
}

static int validate_input(struct link_context *context, int input_index) {
  struct link_input *input = &context->inputs[input_index];
  if (input->size < sizeof(struct elf64_header)) return -1;
  input->header = (const struct elf64_header *)input->bytes;
  const struct elf64_header *header = input->header;
  if (memcmp(header->ident, "\177ELF\2\1\1", 7) || header->type != ET_REL ||
      header->machine != EM_X86_64 || header->version != 1 ||
      header->ehsize != sizeof(*header) ||
      header->shentsize != sizeof(struct elf64_section) || !header->shnum ||
      header->shnum > VIX_LINK_MAX_SECTIONS ||
      !range_valid(header->shoff,
                   (uint64_t)header->shnum * sizeof(struct elf64_section),
                   input->size))
    return -1;
  input->sections =
      (const struct elf64_section *)(input->bytes + header->shoff);
  input->symtab_index = -1;
  for (int index = 0; index < header->shnum; index++) {
    input->mappings[index].category = -1;
    const struct elf64_section *section = &input->sections[index];
    if (section->type != SHT_NOBITS &&
        !range_valid(section->offset, section->size, input->size))
      return -1;
    if (section->type != SHT_NOBITS && section->size) {
      for (int previous = 0; previous < index; previous++) {
        const struct elf64_section *other = &input->sections[previous];
        if (other->type == SHT_NOBITS || !other->size) continue;
        if (section->offset < other->offset + other->size &&
            other->offset < section->offset + section->size)
          return -1;
      }
    }
    if (section->type == SHT_SYMTAB) {
      if (input->symtab_index >= 0 || section->entry_size != sizeof(struct elf64_symbol) ||
          section->size % sizeof(struct elf64_symbol) || section->link >= header->shnum)
        return -1;
      const struct elf64_section *strings = &input->sections[section->link];
      if (strings->type != SHT_STRTAB ||
          !range_valid(strings->offset, strings->size, input->size))
        return -1;
      input->symtab_index = index;
      input->symbols =
          (const struct elf64_symbol *)(input->bytes + section->offset);
      input->symbol_count = section->size / sizeof(struct elf64_symbol);
      input->strings = (const char *)(input->bytes + strings->offset);
      input->string_size = strings->size;
      if (!input->symbol_count || input->symbol_count > VIX_LINK_MAX_SYMBOLS)
        return -1;
    }
    int category = classify_section(section);
    if (category == -2) return -1;
    if (category >= 0) {
      uint64_t alignment = section->alignment ? section->alignment : 1;
      uint64_t offset;
      if (align_value(context->category_size[category], alignment, &offset) ||
          section->size > VIX_LINK_MAX_OUTPUT_SIZE - offset)
        return -1;
      input->mappings[index].category = category;
      input->mappings[index].offset = offset;
      context->category_size[category] = offset + section->size;
      if (alignment > context->category_alignment[category])
        context->category_alignment[category] = alignment;
    }
  }
  return input->symtab_index < 0 ? -1 : 0;
}

static int add_global_definitions(struct link_context *context, int input_index) {
  struct link_input *input = &context->inputs[input_index];
  for (size_t index = 1; index < input->symbol_count; index++) {
    const struct elf64_symbol *symbol = &input->symbols[index];
    int binding = symbol->info >> 4;
    if (binding != STB_LOCAL && binding != STB_GLOBAL) return -1;
    if (!string_valid(input->strings, input->string_size, symbol->name)) return -1;
    if (binding != STB_GLOBAL || symbol->shndx == SHN_UNDEF) continue;
    const char *name = input->strings + symbol->name;
    if (!*name) return -1;
    for (size_t existing = 0; existing < context->global_count; existing++)
      if (!strcmp(context->globals[existing].name, name)) return -1;
    if (context->global_count == VIX_LINK_MAX_GLOBALS) return -1;
    context->globals[context->global_count++] =
        (struct global_symbol){name, input_index, index};
  }
  return 0;
}

static const struct global_symbol *find_global(const struct link_context *context,
                                                const char *name) {
  for (size_t index = 0; index < context->global_count; index++)
    if (!strcmp(context->globals[index].name, name)) return &context->globals[index];
  return NULL;
}

static int resolve_symbol(const struct link_context *context, int input_index,
                          size_t symbol_index, uint64_t *address) {
  const struct link_input *input = &context->inputs[input_index];
  if (symbol_index >= input->symbol_count) return -1;
  const struct elf64_symbol *symbol = &input->symbols[symbol_index];
  if (symbol->shndx == SHN_UNDEF) {
    if (!string_valid(input->strings, input->string_size, symbol->name)) return -1;
    const struct global_symbol *definition =
        find_global(context, input->strings + symbol->name);
    if (!definition) return -1;
    return resolve_symbol(context, definition->input, definition->symbol, address);
  }
  if (symbol->shndx == SHN_ABS) {
    *address = symbol->value;
    return 0;
  }
  if (symbol->shndx >= input->header->shnum) return -1;
  const struct section_mapping *mapping = &input->mappings[symbol->shndx];
  const struct elf64_section *section = &input->sections[symbol->shndx];
  if (mapping->category < 0 || symbol->value > section->size ||
      symbol->value > UINT64_MAX - mapping->offset ||
      mapping->offset + symbol->value >
          UINT64_MAX - context->category_address[mapping->category])
    return -1;
  *address = context->category_address[mapping->category] + mapping->offset +
             symbol->value;
  return 0;
}

static int check_undefined_symbols(const struct link_context *context) {
  for (int input_index = 0; input_index < 2; input_index++) {
    const struct link_input *input = &context->inputs[input_index];
    for (size_t index = 1; index < input->symbol_count; index++) {
      const struct elf64_symbol *symbol = &input->symbols[index];
      if ((symbol->info >> 4) == STB_GLOBAL && symbol->shndx == SHN_UNDEF) {
        uint64_t address;
        if (resolve_symbol(context, input_index, index, &address)) return -1;
      }
    }
  }
  return 0;
}

static int assign_addresses(struct link_context *context) {
  uint64_t next = VIX_LINK_BASE_ADDRESS;
  context->category_address[OUTPUT_TEXT] = next;
  if (context->category_size[OUTPUT_TEXT] == 0 ||
      align_value(next + context->category_size[OUTPUT_TEXT], VIX_LINK_PAGE_SIZE,
                  &next))
    return -1;
  context->category_address[OUTPUT_RODATA] = next;
  if (align_value(next + context->category_size[OUTPUT_RODATA], VIX_LINK_PAGE_SIZE,
                  &next))
    return -1;
  context->category_address[OUTPUT_DATA] = next;
  uint64_t bss_address;
  if (align_value(next + context->category_size[OUTPUT_DATA],
                  context->category_alignment[OUTPUT_BSS], &bss_address))
    return -1;
  context->category_address[OUTPUT_BSS] = bss_address;
  return 0;
}

static int allocate_and_copy_sections(struct link_context *context) {
  for (int category = 0; category < OUTPUT_BSS; category++) {
    size_t size = (size_t)context->category_size[category];
    context->category_bytes[category] = VIX_LINK_CALLOC(size ? size : 1, 1);
    if (!context->category_bytes[category]) return -1;
  }
  for (int input_index = 0; input_index < 2; input_index++) {
    const struct link_input *input = &context->inputs[input_index];
    for (int section_index = 0; section_index < input->header->shnum;
         section_index++) {
      const struct section_mapping *mapping = &input->mappings[section_index];
      const struct elf64_section *section = &input->sections[section_index];
      if (mapping->category >= 0 && mapping->category != OUTPUT_BSS && section->size)
        memcpy(context->category_bytes[mapping->category] + mapping->offset,
               input->bytes + section->offset, section->size);
    }
  }
  return 0;
}

static int add_signed(uint64_t base, int64_t addend, uint64_t *result) {
  if (addend >= 0) {
    if (base > UINT64_MAX - (uint64_t)addend) return -1;
    *result = base + (uint64_t)addend;
  } else {
    uint64_t magnitude = (uint64_t)(-(addend + 1)) + 1;
    if (base < magnitude) return -1;
    *result = base - magnitude;
  }
  return 0;
}

static int apply_relocations(struct link_context *context, int input_index) {
  const struct link_input *input = &context->inputs[input_index];
  for (int section_index = 0; section_index < input->header->shnum;
       section_index++) {
    const struct elf64_section *relocation_section =
        &input->sections[section_index];
    if (relocation_section->type != SHT_RELA) continue;
    if (relocation_section->entry_size != sizeof(struct elf64_rela) ||
        relocation_section->size % sizeof(struct elf64_rela) ||
        relocation_section->link != (uint32_t)input->symtab_index ||
        relocation_section->info >= input->header->shnum)
      return -1;
    const struct section_mapping *target_mapping =
        &input->mappings[relocation_section->info];
    const struct elf64_section *target_section =
        &input->sections[relocation_section->info];
    if (target_mapping->category < 0 || target_mapping->category == OUTPUT_BSS)
      return -1;
    const struct elf64_rela *relocations =
        (const struct elf64_rela *)(input->bytes + relocation_section->offset);
    size_t count = relocation_section->size / sizeof(*relocations);
    for (size_t index = 0; index < count; index++) {
      const struct elf64_rela *relocation = &relocations[index];
      uint32_t type = (uint32_t)relocation->info;
      size_t symbol = (size_t)(relocation->info >> 32);
      size_t width = type == R_X86_64_64 ? 8 : 4;
      if ((type != R_X86_64_64 && type != R_X86_64_PC32 &&
           type != R_X86_64_PLT32) ||
          relocation->offset > target_section->size ||
          width > target_section->size - relocation->offset)
        return -1;
      uint64_t symbol_address;
      if (resolve_symbol(context, input_index, symbol, &symbol_address)) return -1;
      unsigned char *destination =
          context->category_bytes[target_mapping->category] +
          target_mapping->offset + relocation->offset;
      uint64_t place = context->category_address[target_mapping->category] +
                       target_mapping->offset + relocation->offset;
      if (type == R_X86_64_64) {
        uint64_t value;
        if (add_signed(symbol_address, relocation->addend, &value)) return -1;
        memcpy(destination, &value, sizeof(value));
      } else {
        if (relocation->addend < INT32_MIN || relocation->addend > INT32_MAX ||
            symbol_address > INT64_MAX || place > INT64_MAX)
          return -1;
        int64_t value = (int64_t)symbol_address + relocation->addend -
                        (int64_t)place;
        if (value < INT32_MIN || value > INT32_MAX) return -1;
        int32_t encoded = (int32_t)value;
        memcpy(destination, &encoded, sizeof(encoded));
      }
    }
  }
  return 0;
}

static int write_all(FILE *file, const void *data, size_t size) {
  const unsigned char *bytes = data;
  size_t offset = 0;
  while (offset < size) {
    size_t count = VIX_LINK_FWRITE(bytes + offset, 1, size - offset, file);
    if (!count) return -1;
    offset += count;
  }
  return 0;
}

static int write_output(struct link_context *context, const char *path,
                        uint64_t entry) {
  static const char shstrtab[] =
      "\0.text\0.rodata\0.data\0.bss\0.shstrtab\0";
  const uint32_t section_names[OUTPUT_SECTION_COUNT] = {0, 1, 7, 15, 21, 26};
  uint64_t text_offset = VIX_LINK_PAGE_SIZE;
  uint64_t rodata_offset, data_offset, shstrtab_offset, shoff, total;
  if (align_value(text_offset + context->category_size[OUTPUT_TEXT],
                  VIX_LINK_PAGE_SIZE, &rodata_offset) ||
      align_value(rodata_offset + context->category_size[OUTPUT_RODATA],
                  VIX_LINK_PAGE_SIZE, &data_offset) ||
      data_offset + context->category_size[OUTPUT_DATA] > UINT64_MAX - sizeof(shstrtab))
    return -1;
  shstrtab_offset = data_offset + context->category_size[OUTPUT_DATA];
  if (align_value(shstrtab_offset + sizeof(shstrtab), 8, &shoff) ||
      shoff > VIX_LINK_MAX_OUTPUT_SIZE -
                  OUTPUT_SECTION_COUNT * sizeof(struct elf64_section))
    return -1;
  total = shoff + OUTPUT_SECTION_COUNT * sizeof(struct elf64_section);

  unsigned char *output = VIX_LINK_CALLOC((size_t)total, 1);
  if (!output) return -1;
  struct elf64_header *header = (struct elf64_header *)output;
  struct elf64_program *programs =
      (struct elf64_program *)(output + sizeof(*header));
  struct elf64_section *sections =
      (struct elf64_section *)(output + shoff);
  memcpy(header->ident, "\177ELF\2\1\1", 7);
  header->type = ET_EXEC;
  header->machine = EM_X86_64;
  header->version = 1;
  header->entry = entry;
  header->phoff = sizeof(*header);
  header->shoff = shoff;
  header->ehsize = sizeof(*header);
  header->phentsize = sizeof(*programs);
  header->phnum = 3;
  header->shentsize = sizeof(*sections);
  header->shnum = OUTPUT_SECTION_COUNT;
  header->shstrndx = 5;

  programs[0] = (struct elf64_program){
      PT_LOAD, PF_R | PF_X, text_offset, context->category_address[OUTPUT_TEXT],
      context->category_address[OUTPUT_TEXT], context->category_size[OUTPUT_TEXT],
      context->category_size[OUTPUT_TEXT], VIX_LINK_PAGE_SIZE};
  programs[1] = (struct elf64_program){
      PT_LOAD, PF_R, rodata_offset, context->category_address[OUTPUT_RODATA],
      context->category_address[OUTPUT_RODATA],
      context->category_size[OUTPUT_RODATA],
      context->category_size[OUTPUT_RODATA], VIX_LINK_PAGE_SIZE};
  uint64_t data_memory_size =
      context->category_address[OUTPUT_BSS] -
          context->category_address[OUTPUT_DATA] +
      context->category_size[OUTPUT_BSS];
  programs[2] = (struct elf64_program){
      PT_LOAD, PF_R | PF_W, data_offset, context->category_address[OUTPUT_DATA],
      context->category_address[OUTPUT_DATA], context->category_size[OUTPUT_DATA],
      data_memory_size, VIX_LINK_PAGE_SIZE};

#define OUTPUT_SECTION(INDEX, TYPE, FLAGS, ADDRESS, OFFSET, SIZE, ALIGN)        \
  sections[INDEX] = (struct elf64_section){                                    \
      section_names[INDEX], TYPE, FLAGS, ADDRESS, OFFSET, SIZE, 0, 0, ALIGN, 0}
  OUTPUT_SECTION(1, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR,
                 context->category_address[OUTPUT_TEXT], text_offset,
                 context->category_size[OUTPUT_TEXT], 16);
  OUTPUT_SECTION(2, SHT_PROGBITS, SHF_ALLOC,
                 context->category_address[OUTPUT_RODATA], rodata_offset,
                 context->category_size[OUTPUT_RODATA], 16);
  OUTPUT_SECTION(3, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE,
                 context->category_address[OUTPUT_DATA], data_offset,
                 context->category_size[OUTPUT_DATA], 16);
  OUTPUT_SECTION(4, SHT_NOBITS, SHF_ALLOC | SHF_WRITE,
                 context->category_address[OUTPUT_BSS],
                 data_offset + context->category_size[OUTPUT_DATA],
                 context->category_size[OUTPUT_BSS], 16);
  OUTPUT_SECTION(5, SHT_STRTAB, 0, 0, shstrtab_offset, sizeof(shstrtab), 1);
#undef OUTPUT_SECTION
  memcpy(output + text_offset, context->category_bytes[OUTPUT_TEXT],
         context->category_size[OUTPUT_TEXT]);
  memcpy(output + rodata_offset, context->category_bytes[OUTPUT_RODATA],
         context->category_size[OUTPUT_RODATA]);
  memcpy(output + data_offset, context->category_bytes[OUTPUT_DATA],
         context->category_size[OUTPUT_DATA]);
  memcpy(output + shstrtab_offset, shstrtab, sizeof(shstrtab));

  char temporary[512];
  int name_length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
  if (name_length < 0 || (size_t)name_length >= sizeof(temporary)) {
    VIX_LINK_FREE(output);
    return -1;
  }
  remove(temporary);
  int descriptor = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (descriptor < 0) {
    VIX_LINK_FREE(output);
    return -1;
  }
  FILE *file = fdopen(descriptor, "wb");
  if (!file) {
    close(descriptor);
    remove(temporary);
    VIX_LINK_FREE(output);
    return -1;
  }
  int result = write_all(file, output, (size_t)total) || fflush(file) ||
               fsync(fileno(file));
  if (fclose(file)) result = 1;
  if (!result && rename(temporary, path)) result = 1;
  if (result) remove(temporary);
  VIX_LINK_FREE(output);
  return result ? -1 : 0;
}

static void release_context(struct link_context *context) {
  for (int index = 0; index < 2; index++)
    VIX_LINK_FREE(context->inputs[index].bytes);
  for (int category = 0; category < OUTPUT_BSS; category++)
    VIX_LINK_FREE(context->category_bytes[category]);
}

int vix_elf_link_static(const char *object_path, const char *runtime_path,
                        const char *output_path) {
  if (!object_path || !runtime_path || !output_path || !*object_path ||
      !*runtime_path || !*output_path) {
    errno = EINVAL;
    return -1;
  }
  struct link_context context;
  memset(&context, 0, sizeof(context));
  for (int category = 0; category < OUTPUT_CATEGORY_COUNT; category++)
    context.category_alignment[category] = 1;
  int result = read_input_file(object_path, &context.inputs[0]) ||
               read_input_file(runtime_path, &context.inputs[1]) ||
               validate_input(&context, 0) || validate_input(&context, 1) ||
               add_global_definitions(&context, 0) ||
               add_global_definitions(&context, 1) || assign_addresses(&context) ||
               check_undefined_symbols(&context) ||
               allocate_and_copy_sections(&context) ||
               apply_relocations(&context, 0) || apply_relocations(&context, 1);
  uint64_t entry = 0;
  const struct global_symbol *start = find_global(&context, "_start");
  if (!result && (!start || resolve_symbol(&context, start->input, start->symbol,
                                            &entry) ||
                  write_output(&context, output_path, entry)))
    result = 1;
  release_context(&context);
  if (result) {
    errno = ENOEXEC;
    return -1;
  }
  return 0;
}
