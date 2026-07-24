#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef INT8_MIN
#define INT8_MIN (-INT8_MAX - 1)
#endif
#ifndef INT32_MIN
#define INT32_MIN (-INT32_MAX - 1)
#endif
#ifndef INT64_MIN
#define INT64_MIN (-INT64_MAX - 1)
#endif

#ifndef VIX_ELF_MALLOC
#define VIX_ELF_MALLOC malloc
#endif
#ifndef VIX_ELF_REALLOC
#define VIX_ELF_REALLOC realloc
#endif
#ifndef VIX_ELF_FREE
#define VIX_ELF_FREE free
#endif
#ifndef VIX_ELF_FOPEN
#define VIX_ELF_FOPEN fopen
#endif
#ifndef VIX_ELF_FWRITE
#define VIX_ELF_FWRITE fwrite
#endif
#ifndef VIX_ELF_FCLOSE
#define VIX_ELF_FCLOSE fclose
#endif
#ifndef VIX_ELF_REMOVE
#define VIX_ELF_REMOVE remove
#endif

#define VIX_ELF_MAX_SYMBOLS 512
#define VIX_ELF_MAX_FIXUPS 1024
#define VIX_ELF_MAX_RELOCS 512
#define VIX_ELF_MAX_OUTPUT (1024u * 1024u)

enum {
  SEC_UNDEF = 0,
  SEC_TEXT = 1,
  SEC_RELA_TEXT = 2,
  SEC_RODATA = 3,
  SEC_DATA = 4,
  SEC_SYMTAB = 5,
  SEC_STRTAB = 6,
  SEC_SHSTRTAB = 7,
  SEC_COUNT = 8,
};

enum {
  SHT_NULL = 0,
  SHT_PROGBITS = 1,
  SHT_SYMTAB = 2,
  SHT_STRTAB = 3,
  SHT_RELA = 4,
  SHF_WRITE = 1,
  SHF_ALLOC = 2,
  SHF_EXECINSTR = 4,
  STB_LOCAL = 0,
  STB_GLOBAL = 1,
  STT_NOTYPE = 0,
  STT_OBJECT = 1,
  STT_FUNC = 2,
  R_X86_64_PC32 = 2,
  R_X86_64_PLT32 = 4,
};

struct byte_buffer {
  unsigned char *data;
  size_t size;
  size_t capacity;
};

struct assembler_symbol {
  char name[96];
  uint64_t value;
  int section;
  int defined;
  int global;
  int function;
  int output_index;
};

struct branch_fixup {
  size_t offset;
  int symbol;
};

struct object_relocation {
  uint64_t offset;
  int symbol;
  uint32_t type;
  int64_t addend;
};

struct assembler {
  struct byte_buffer text;
  struct byte_buffer rodata;
  struct assembler_symbol symbols[VIX_ELF_MAX_SYMBOLS];
  struct branch_fixup fixups[VIX_ELF_MAX_FIXUPS];
  struct object_relocation relocations[VIX_ELF_MAX_RELOCS];
  int symbol_count;
  int fixup_count;
  int relocation_count;
  int section;
  int failed;
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

_Static_assert(sizeof(struct elf64_header) == 64, "ELF64 header size");
_Static_assert(sizeof(struct elf64_section) == 64, "ELF64 section size");
_Static_assert(sizeof(struct elf64_symbol) == 24, "ELF64 symbol size");
_Static_assert(sizeof(struct elf64_rela) == 24, "ELF64 rela size");

static int buffer_reserve(struct byte_buffer *buffer, size_t additional) {
  if (additional > VIX_ELF_MAX_OUTPUT - buffer->size) return -1;
  size_t needed = buffer->size + additional;
  if (needed <= buffer->capacity) return 0;
  size_t capacity = buffer->capacity ? buffer->capacity : 256;
  while (capacity < needed) {
    if (capacity > VIX_ELF_MAX_OUTPUT / 2) {
      capacity = VIX_ELF_MAX_OUTPUT;
      break;
    }
    capacity *= 2;
  }
  void *grown = VIX_ELF_REALLOC(buffer->data, capacity);
  if (!grown) return -1;
  buffer->data = grown;
  buffer->capacity = capacity;
  return 0;
}

static int buffer_bytes(struct byte_buffer *buffer, const void *data,
                        size_t size) {
  if (buffer_reserve(buffer, size)) return -1;
  memcpy(buffer->data + buffer->size, data, size);
  buffer->size += size;
  return 0;
}

static int buffer_u8(struct byte_buffer *buffer, uint8_t value) {
  return buffer_bytes(buffer, &value, 1);
}

static int buffer_u32(struct byte_buffer *buffer, uint32_t value) {
  return buffer_bytes(buffer, &value, 4);
}

static int buffer_u64(struct byte_buffer *buffer, uint64_t value) {
  return buffer_bytes(buffer, &value, 8);
}

static char *trim(char *text) {
  while (*text == ' ' || *text == '\t' || *text == '\r') text++;
  size_t length = strlen(text);
  while (length && (text[length - 1] == ' ' || text[length - 1] == '\t' ||
                    text[length - 1] == '\r'))
    text[--length] = '\0';
  return text;
}

static int parse_integer(const char *text, int64_t *value) {
  int negative = 0;
  uint64_t result = 0;
  int base = 10;
  if (*text == '-') {
    negative = 1;
    text++;
  } else if (*text == '+') text++;
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    base = 16;
    text += 2;
  }
  if (!*text) return -1;
  while (*text) {
    int digit;
    if (*text >= '0' && *text <= '9') digit = *text - '0';
    else if (*text >= 'a' && *text <= 'f') digit = *text - 'a' + 10;
    else if (*text >= 'A' && *text <= 'F') digit = *text - 'A' + 10;
    else return -1;
    if (digit >= base || result > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base)
      return -1;
    result = result * (uint64_t)base + (uint64_t)digit;
    text++;
  }
  uint64_t limit = negative ? (uint64_t)INT64_MAX + 1 : (uint64_t)INT64_MAX;
  if (result > limit) return -1;
  if (negative && result == (uint64_t)INT64_MAX + 1) *value = INT64_MIN;
  else *value = negative ? -(int64_t)result : (int64_t)result;
  return 0;
}

static int parse_double(const char *text, double *value) {
  int negative = 0;
  int digits = 0;
  double result = 0.0;
  double scale = 1.0;
  if (*text == '-' || *text == '+') {
    negative = *text == '-';
    text++;
  }
  while (*text >= '0' && *text <= '9') {
    result = result * 10.0 + (double)(*text++ - '0');
    digits++;
  }
  if (*text == '.') {
    text++;
    while (*text >= '0' && *text <= '9') {
      scale *= 0.1;
      result += (double)(*text++ - '0') * scale;
      digits++;
    }
  }
  if (!digits || *text) return -1;
  *value = negative ? -result : result;
  return 0;
}

static int find_symbol(struct assembler *assembler, const char *name,
                       int create) {
  for (int index = 0; index < assembler->symbol_count; index++)
    if (!strcmp(assembler->symbols[index].name, name)) return index;
  if (!create || assembler->symbol_count == VIX_ELF_MAX_SYMBOLS) return -1;
  size_t length = strlen(name);
  if (!length || length >= sizeof(assembler->symbols[0].name)) return -1;
  int index = assembler->symbol_count++;
  memset(&assembler->symbols[index], 0, sizeof(assembler->symbols[index]));
  memcpy(assembler->symbols[index].name, name, length + 1);
  return index;
}

static int define_symbol(struct assembler *assembler, const char *name) {
  int index = find_symbol(assembler, name, 1);
  if (index < 0 || assembler->symbols[index].defined) return -1;
  assembler->symbols[index].defined = 1;
  assembler->symbols[index].section = assembler->section;
  assembler->symbols[index].value = assembler->section == SEC_TEXT
                                          ? assembler->text.size
                                          : assembler->rodata.size;
  return 0;
}

struct register_info {
  int code;
  int width;
  int kind;
};

enum { REGISTER_GPR, REGISTER_XMM };

static int register_info(const char *name, struct register_info *info) {
  static const char *r64[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp",
                              "rsi", "rdi", "r8", "r9", "r10", "r11"};
  static const char *r32[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp",
                              "esi", "edi", "r8d", "r9d", "r10d", "r11d"};
  static const char *r8[] = {"al", "cl", "dl", "bl"};
  for (int index = 0; index < 12; index++) {
    if (!strcmp(name, r64[index])) {
      info->code = index;
      info->width = 64;
      info->kind = REGISTER_GPR;
      return 0;
    }
    if (!strcmp(name, r32[index])) {
      info->code = index;
      info->width = 32;
      info->kind = REGISTER_GPR;
      return 0;
    }
  }
  for (int index = 0; index < 4; index++)
    if (!strcmp(name, r8[index])) {
      info->code = index;
      info->width = 8;
      info->kind = REGISTER_GPR;
      return 0;
    }
  static const char *extended_r8[] = {"spl", "bpl", "sil", "dil", "r8b",
                                      "r9b", "r10b", "r11b"};
  for (int index = 0; index < 8; index++)
    if (!strcmp(name, extended_r8[index])) {
      info->code = index + 4;
      info->width = 8;
      info->kind = REGISTER_GPR;
      return 0;
    }
  if (!strncmp(name, "xmm", 3)) {
    int64_t code;
    if (!parse_integer(name + 3, &code) && code >= 0 && code <= 15) {
      info->code = (int)code;
      info->width = 64;
      info->kind = REGISTER_XMM;
      return 0;
    }
  }
  return -1;
}

enum operand_kind { OPERAND_INVALID, OPERAND_REGISTER, OPERAND_MEMORY,
                    OPERAND_IMMEDIATE, OPERAND_SYMBOL };

struct operand {
  enum operand_kind kind;
  struct register_info reg;
  int width;
  int base;
  int displacement;
  int symbol;
  int64_t immediate;
};

static int parse_operand(struct assembler *assembler, char *text,
                         struct operand *operand) {
  memset(operand, 0, sizeof(*operand));
  text = trim(text);
  if (!strncmp(text, "byte ", 5)) { operand->width = 8; text = trim(text + 5); }
  else if (!strncmp(text, "dword ", 6)) { operand->width = 32; text = trim(text + 6); }
  else if (!strncmp(text, "qword ", 6)) { operand->width = 64; text = trim(text + 6); }
  if (!register_info(text, &operand->reg)) {
    operand->kind = OPERAND_REGISTER;
    if (!operand->width) operand->width = operand->reg.width;
    return 0;
  }
  if (*text == '[') {
    size_t length = strlen(text);
    if (length < 3 || text[length - 1] != ']') return -1;
    text[length - 1] = '\0';
    char *inside = trim(text + 1);
    if (!strncmp(inside, "rel ", 4)) inside = trim(inside + 4);
    char *sign = NULL;
    for (char *cursor = inside + 1; *cursor; cursor++)
      if (*cursor == '+' || *cursor == '-') { sign = cursor; break; }
    char sign_value = sign ? *sign : '\0';
    if (sign) *sign = '\0';
    struct register_info base;
    if (!register_info(trim(inside), &base) && base.kind == REGISTER_GPR &&
        base.width == 64) {
      operand->kind = OPERAND_MEMORY;
      operand->base = base.code;
      operand->displacement = 0;
      if (sign) {
        int64_t displacement;
        if (parse_integer(trim(sign + 1), &displacement) ||
            displacement > (sign_value == '-' ? (int64_t)INT32_MAX + 1
                                               : INT32_MAX))
          return -1;
        operand->displacement = (int)displacement;
        if (sign_value == '-') operand->displacement = -operand->displacement;
      }
      return 0;
    }
    if (sign) *sign = '\0';
    operand->symbol = find_symbol(assembler, trim(inside), 1);
    if (operand->symbol < 0) return -1;
    operand->kind = OPERAND_SYMBOL;
    return 0;
  }
  if (!parse_integer(text, &operand->immediate)) {
    operand->kind = OPERAND_IMMEDIATE;
    return 0;
  }
  operand->symbol = find_symbol(assembler, text, 1);
  if (operand->symbol < 0) return -1;
  operand->kind = OPERAND_SYMBOL;
  return 0;
}

static int emit_rex(struct byte_buffer *buffer, int width, int reg, int base) {
  int rex = 0x40 | (width == 64 ? 8 : 0) | ((reg >> 3) & 1) * 4 |
            ((base >> 3) & 1);
  return rex != 0x40 || (width == 8 && (reg >= 4 || base >= 4))
             ? buffer_u8(buffer, (uint8_t)rex)
             : 0;
}

static int emit_modrm(struct byte_buffer *buffer, int reg,
                      const struct operand *operand) {
  if (operand->kind == OPERAND_REGISTER)
    return buffer_u8(buffer, (uint8_t)(0xc0 | ((reg & 7) << 3) |
                                       (operand->reg.code & 7)));
  if (operand->kind != OPERAND_MEMORY) return -1;
  int base = operand->base & 7;
  int displacement = operand->displacement;
  int mod = displacement == 0 && base != 5 ? 0 :
            (displacement >= -128 && displacement <= 127 ? 1 : 2);
  if (buffer_u8(buffer, (uint8_t)((mod << 6) | ((reg & 7) << 3) |
                                  (base == 4 ? 4 : base)))) return -1;
  if (base == 4 && buffer_u8(buffer, (uint8_t)(0x20 | base))) return -1;
  if (mod == 1) return buffer_u8(buffer, (uint8_t)displacement);
  if (mod == 2 || (mod == 0 && base == 5))
    return buffer_u32(buffer, (uint32_t)displacement);
  return 0;
}

static int emit_reg_rm(struct assembler *assembler, uint8_t opcode,
                       const struct register_info *reg,
                       const struct operand *rm, int width) {
  if (rm->kind != OPERAND_REGISTER && rm->kind != OPERAND_MEMORY) return -1;
  if (reg->kind != REGISTER_GPR ||
      (rm->kind == OPERAND_REGISTER && rm->reg.kind != REGISTER_GPR)) return -1;
  int base = rm->kind == OPERAND_REGISTER ? rm->reg.code : rm->base;
  if (emit_rex(&assembler->text, width, reg->code, base) ||
      buffer_u8(&assembler->text, opcode) ||
      emit_modrm(&assembler->text, reg->code, rm)) return -1;
  return 0;
}

static int emit_mov(struct assembler *assembler, struct operand *left,
                    struct operand *right) {
  int width = left->width ? left->width : right->width;
  if (left->kind == OPERAND_REGISTER && right->kind == OPERAND_IMMEDIATE) {
    if (left->reg.kind != REGISTER_GPR) return -1;
    if (emit_rex(&assembler->text, width, 0, left->reg.code) ||
        buffer_u8(&assembler->text,
                  (uint8_t)((width == 8 ? 0xb0 : 0xb8) +
                            (left->reg.code & 7))))
      return -1;
    return width == 64 ? buffer_u64(&assembler->text, (uint64_t)right->immediate) :
           width == 8 ? buffer_u8(&assembler->text, (uint8_t)right->immediate) :
                        buffer_u32(&assembler->text, (uint32_t)right->immediate);
  }
  if (left->kind == OPERAND_REGISTER &&
      (right->kind == OPERAND_REGISTER || right->kind == OPERAND_MEMORY)) {
    if (left->reg.kind != REGISTER_GPR ||
        (right->kind == OPERAND_REGISTER && right->reg.width != width)) return -1;
    return emit_reg_rm(assembler, width == 8 ? 0x8a : 0x8b, &left->reg,
                       right, width);
  }
  if ((left->kind == OPERAND_REGISTER || left->kind == OPERAND_MEMORY) &&
      right->kind == OPERAND_REGISTER) {
    if (right->reg.kind != REGISTER_GPR ||
        (left->kind == OPERAND_REGISTER && left->reg.width != width)) return -1;
    return emit_reg_rm(assembler, width == 8 ? 0x88 : 0x89, &right->reg,
                       left, width);
  }
  if (left->kind == OPERAND_MEMORY && right->kind == OPERAND_IMMEDIATE) {
    if ((width == 64 && (right->immediate < INT32_MIN ||
                         right->immediate > INT32_MAX)) ||
        (width == 32 && (right->immediate < INT32_MIN ||
                         (uint64_t)right->immediate > UINT32_MAX)) ||
        (width == 8 && (right->immediate < INT8_MIN ||
                        right->immediate > UINT8_MAX))) return -1;
    struct register_info extension = {0, width, REGISTER_GPR};
    if (emit_rex(&assembler->text, width, 0, left->base) ||
        buffer_u8(&assembler->text, width == 8 ? 0xc6 : 0xc7) ||
        emit_modrm(&assembler->text, extension.code, left)) return -1;
    return width == 8 ? buffer_u8(&assembler->text, (uint8_t)right->immediate)
                      : buffer_u32(&assembler->text, (uint32_t)right->immediate);
  }
  return -1;
}

static int emit_binary(struct assembler *assembler, const char *mnemonic,
                       struct operand *left, struct operand *right) {
  int extension = !strcmp(mnemonic, "add") ? 0 :
                  !strcmp(mnemonic, "sub") ? 5 :
                  !strcmp(mnemonic, "and") ? 4 : 7;
  uint8_t opcode = !strcmp(mnemonic, "add") ? 0x03 :
                   !strcmp(mnemonic, "sub") ? 0x2b :
                   !strcmp(mnemonic, "and") ? (left->width == 8 ? 0x22 : 0x23)
                                               : 0x3b;
  int width = left->width;
  if (left->kind != OPERAND_REGISTER || left->reg.kind != REGISTER_GPR) return -1;
  if (right->kind == OPERAND_IMMEDIATE) {
    if (emit_rex(&assembler->text, width, extension, left->reg.code) ||
        buffer_u8(&assembler->text, width == 8 ? 0x80 : 0x81)) return -1;
    struct operand destination = *left;
    if (emit_modrm(&assembler->text, extension, &destination)) return -1;
    return width == 8 ? buffer_u8(&assembler->text, (uint8_t)right->immediate)
                      : buffer_u32(&assembler->text, (uint32_t)right->immediate);
  }
  return emit_reg_rm(assembler, opcode, &left->reg, right, width);
}

static int add_fixup(struct assembler *assembler, int symbol, size_t offset) {
  if (assembler->fixup_count == VIX_ELF_MAX_FIXUPS) return -1;
  assembler->fixups[assembler->fixup_count++] =
      (struct branch_fixup){offset, symbol};
  return 0;
}

static int add_relocation(struct assembler *assembler, int symbol,
                          size_t offset, uint32_t type, int64_t addend) {
  if (assembler->relocation_count == VIX_ELF_MAX_RELOCS) return -1;
  assembler->relocations[assembler->relocation_count++] =
      (struct object_relocation){offset, symbol, type, addend};
  return 0;
}

static int emit_instruction(struct assembler *assembler, char *line) {
  char *space = line;
  while (*space && *space != ' ' && *space != '\t') space++;
  char *mnemonic = line;
  char *operands = space;
  if (*space) *space++ = '\0';
  operands = trim(space);

  if (!strcmp(mnemonic, "ret")) return buffer_u8(&assembler->text, 0xc3);
  if (!strcmp(mnemonic, "cdq")) return buffer_u8(&assembler->text, 0x99);
  if (!strcmp(mnemonic, "syscall")) {
    return buffer_u8(&assembler->text, 0x0f) ||
           buffer_u8(&assembler->text, 0x05) ? -1 : 0;
  }
  if (!strcmp(mnemonic, "push") || !strcmp(mnemonic, "pop")) {
    struct register_info reg;
    if (register_info(operands, &reg) || reg.kind != REGISTER_GPR ||
        reg.width != 64) return -1;
    if (reg.code >= 8 && buffer_u8(&assembler->text, 0x41)) return -1;
    return buffer_u8(&assembler->text,
                     (uint8_t)((!strcmp(mnemonic, "push") ? 0x50 : 0x58) +
                               (reg.code & 7)));
  }
  if (!strcmp(mnemonic, "jmp") || !strcmp(mnemonic, "je") ||
      !strcmp(mnemonic, "jnz") || !strcmp(mnemonic, "jne") ||
      !strcmp(mnemonic, "jb") || !strcmp(mnemonic, "js")) {
    int symbol = find_symbol(assembler, operands, 1);
    if (symbol < 0) return -1;
    if (!strcmp(mnemonic, "jmp")) {
      if (buffer_u8(&assembler->text, 0xe9)) return -1;
    } else {
      uint8_t condition = !strcmp(mnemonic, "je") ? 0x84 :
                          (!strcmp(mnemonic, "jnz") || !strcmp(mnemonic, "jne"))
                              ? 0x85 : !strcmp(mnemonic, "jb") ? 0x82 : 0x88;
      if (buffer_u8(&assembler->text, 0x0f) ||
          buffer_u8(&assembler->text, condition)) return -1;
    }
    size_t offset = assembler->text.size;
    return buffer_u32(&assembler->text, 0) || add_fixup(assembler, symbol, offset)
               ? -1 : 0;
  }
  if (!strcmp(mnemonic, "call")) {
    int symbol = find_symbol(assembler, operands, 1);
    if (symbol < 0 || buffer_u8(&assembler->text, 0xe8)) return -1;
    size_t offset = assembler->text.size;
    if (buffer_u32(&assembler->text, 0)) return -1;
    return add_relocation(assembler, symbol, offset, R_X86_64_PLT32, -4);
  }
  if (!strncmp(mnemonic, "set", 3)) {
    struct operand destination;
    uint8_t condition = !strcmp(mnemonic, "sete") ? 0x94 :
                        !strcmp(mnemonic, "setne") ? 0x95 :
                        !strcmp(mnemonic, "setl") ? 0x9c :
                        !strcmp(mnemonic, "setle") ? 0x9e :
                        !strcmp(mnemonic, "setg") ? 0x9f :
                        !strcmp(mnemonic, "setge") ? 0x9d :
                        !strcmp(mnemonic, "setnp") ? 0x9b : 0;
    if (!condition || parse_operand(assembler, operands, &destination) ||
        destination.kind != OPERAND_REGISTER ||
        destination.reg.kind != REGISTER_GPR || destination.width != 8)
      return -1;
    return emit_rex(&assembler->text, 8, 0, destination.reg.code) ||
           buffer_u8(&assembler->text, 0x0f) ||
           buffer_u8(&assembler->text, condition) ||
           buffer_u8(&assembler->text,
                     (uint8_t)(0xc0 | (destination.reg.code & 7))) ? -1 : 0;
  }
  if (!strcmp(mnemonic, "idiv")) {
    struct operand source;
    if (parse_operand(assembler, operands, &source) ||
        source.kind != OPERAND_REGISTER || source.reg.kind != REGISTER_GPR ||
        (source.width != 32 && source.width != 64)) return -1;
    if (emit_rex(&assembler->text, source.width, 7, source.reg.code) ||
        buffer_u8(&assembler->text, 0xf7) ||
        emit_modrm(&assembler->text, 7, &source)) return -1;
    return 0;
  }

  char *comma = NULL;
  int brackets = 0;
  for (char *cursor = operands; *cursor; cursor++) {
    if (*cursor == '[') brackets++;
    else if (*cursor == ']') brackets--;
    else if (*cursor == ',' && !brackets) { comma = cursor; break; }
  }
  if (!comma) return -1;
  *comma = '\0';
  struct operand left, right;
  if (parse_operand(assembler, operands, &left) ||
      parse_operand(assembler, comma + 1, &right)) return -1;

  if (!strcmp(mnemonic, "mov")) return emit_mov(assembler, &left, &right);
  if (!strcmp(mnemonic, "add") || !strcmp(mnemonic, "sub") ||
      !strcmp(mnemonic, "cmp") || !strcmp(mnemonic, "and"))
    return emit_binary(assembler, mnemonic, &left, &right);
  if (!strcmp(mnemonic, "imul")) {
    if (left.kind != OPERAND_REGISTER || left.reg.kind != REGISTER_GPR ||
        right.kind != OPERAND_IMMEDIATE)
      return -1;
    if (emit_rex(&assembler->text, left.width, left.reg.code, left.reg.code) ||
        buffer_u8(&assembler->text, 0x69)) return -1;
    struct operand source = left;
    if (emit_modrm(&assembler->text, left.reg.code, &source)) return -1;
    return buffer_u32(&assembler->text, (uint32_t)right.immediate);
  }
  if (!strcmp(mnemonic, "lea")) {
    if (left.kind != OPERAND_REGISTER) return -1;
    if (right.kind == OPERAND_SYMBOL) {
      if (emit_rex(&assembler->text, 64, left.reg.code, 5) ||
          buffer_u8(&assembler->text, 0x8d) ||
          buffer_u8(&assembler->text,
                    (uint8_t)(((left.reg.code & 7) << 3) | 5))) return -1;
      size_t offset = assembler->text.size;
      if (buffer_u32(&assembler->text, 0)) return -1;
      return add_relocation(assembler, right.symbol, offset, R_X86_64_PC32, -4);
    }
    return emit_reg_rm(assembler, 0x8d, &left.reg, &right, 64);
  }
  if (!strcmp(mnemonic, "movzx")) {
    if (left.kind != OPERAND_REGISTER || left.reg.kind != REGISTER_GPR ||
        (right.kind != OPERAND_REGISTER && right.kind != OPERAND_MEMORY))
      return -1;
    if (right.kind == OPERAND_REGISTER &&
        (right.reg.kind != REGISTER_GPR || right.width != 8)) return -1;
    int base = right.kind == OPERAND_REGISTER ? right.reg.code : right.base;
    if (emit_rex(&assembler->text, left.width == 64 ? 64 : 32,
                 left.reg.code, base) || buffer_u8(&assembler->text, 0x0f) ||
        buffer_u8(&assembler->text, 0xb6) ||
        emit_modrm(&assembler->text, left.reg.code, &right)) return -1;
    return 0;
  }
  if (!strcmp(mnemonic, "movq")) {
    if (left.kind == OPERAND_REGISTER && left.reg.kind == REGISTER_XMM &&
        (right.kind == OPERAND_REGISTER || right.kind == OPERAND_MEMORY)) {
      if (right.kind == OPERAND_REGISTER && right.reg.kind != REGISTER_GPR)
        return -1;
      int base = right.kind == OPERAND_REGISTER ? right.reg.code : right.base;
      return buffer_u8(&assembler->text, 0xf3) ||
             emit_rex(&assembler->text, 64, left.reg.code, base) ||
             buffer_u8(&assembler->text, 0x0f) ||
             buffer_u8(&assembler->text, 0x7e) ||
             emit_modrm(&assembler->text, left.reg.code, &right) ? -1 : 0;
    }
    if ((left.kind == OPERAND_REGISTER || left.kind == OPERAND_MEMORY) &&
        right.kind == OPERAND_REGISTER && right.reg.kind == REGISTER_XMM) {
      if (left.kind == OPERAND_REGISTER && left.reg.kind != REGISTER_GPR)
        return -1;
      int base = left.kind == OPERAND_REGISTER ? left.reg.code : left.base;
      return buffer_u8(&assembler->text, 0x66) ||
             emit_rex(&assembler->text, 64, right.reg.code, base) ||
             buffer_u8(&assembler->text, 0x0f) ||
             buffer_u8(&assembler->text, 0xd6) ||
             emit_modrm(&assembler->text, right.reg.code, &left) ? -1 : 0;
    }
    return -1;
  }
  if (!strcmp(mnemonic, "movsd") || !strcmp(mnemonic, "addsd") ||
      !strcmp(mnemonic, "ucomisd")) {
    if (left.kind != OPERAND_REGISTER || left.reg.kind != REGISTER_XMM) return -1;
    uint8_t prefix = !strcmp(mnemonic, "ucomisd") ? 0x66 : 0xf2;
    uint8_t opcode = !strcmp(mnemonic, "movsd") ? 0x10 :
                     !strcmp(mnemonic, "addsd") ? 0x58 : 0x2e;
    if (right.kind == OPERAND_SYMBOL) {
      if (strcmp(mnemonic, "movsd") || buffer_u8(&assembler->text, prefix) ||
          emit_rex(&assembler->text, 32, left.reg.code, 5) ||
          buffer_u8(&assembler->text, 0x0f) ||
          buffer_u8(&assembler->text, opcode) ||
          buffer_u8(&assembler->text,
                    (uint8_t)(((left.reg.code & 7) << 3) | 5))) return -1;
      size_t offset = assembler->text.size;
      if (buffer_u32(&assembler->text, 0)) return -1;
      return add_relocation(assembler, right.symbol, offset, R_X86_64_PC32, -4);
    }
    if (right.kind != OPERAND_REGISTER && right.kind != OPERAND_MEMORY) return -1;
    if (right.kind == OPERAND_REGISTER && right.reg.kind != REGISTER_XMM) return -1;
    int base = right.kind == OPERAND_REGISTER ? right.reg.code : right.base;
    return buffer_u8(&assembler->text, prefix) ||
           emit_rex(&assembler->text, 32, left.reg.code, base) ||
           buffer_u8(&assembler->text, 0x0f) ||
           buffer_u8(&assembler->text, opcode) ||
           emit_modrm(&assembler->text, left.reg.code, &right) ? -1 : 0;
  }
  return -1;
}

static int parse_data(struct assembler *assembler, char *text) {
  char *colon = strchr(text, ':');
  if (!colon) return -1;
  *colon = '\0';
  if (define_symbol(assembler, trim(text))) return -1;
  char *directive = trim(colon + 1);
  int unit;
  if (!strncmp(directive, "db ", 3)) unit = 1;
  else if (!strncmp(directive, "dq ", 3)) unit = 8;
  else return -1;
  char *cursor = directive + 3;
  while (*cursor) {
    char *comma = strchr(cursor, ',');
    if (comma) *comma = '\0';
    char *item = trim(cursor);
    if (!*item) return -1;
    if (unit == 1) {
      int64_t value;
      if (parse_integer(item, &value) || value < 0 || value > 255 ||
          buffer_u8(&assembler->rodata, (uint8_t)value)) return -1;
    } else {
      int64_t integer;
      uint64_t bits;
      if (!parse_integer(item, &integer)) bits = (uint64_t)integer;
      else {
        double value;
        if (parse_double(item, &value)) return -1;
        memcpy(&bits, &value, sizeof(bits));
      }
      if (buffer_u64(&assembler->rodata, bits)) return -1;
    }
    if (!comma) break;
    cursor = comma + 1;
    if (!*trim(cursor)) return -1;
  }
  return 0;
}

static int resolve_fixups(struct assembler *assembler) {
  for (int index = 0; index < assembler->fixup_count; index++) {
    struct branch_fixup *fixup = &assembler->fixups[index];
    if (fixup->symbol < 0 || fixup->symbol >= assembler->symbol_count)
      return -1;
    struct assembler_symbol *symbol = &assembler->symbols[fixup->symbol];
    if (!symbol->defined || symbol->section != SEC_TEXT ||
        symbol->value > INT32_MAX || fixup->offset > assembler->text.size ||
        assembler->text.size - fixup->offset < 4)
      return -1;
    int64_t displacement = (int64_t)symbol->value - (int64_t)(fixup->offset + 4);
    if (displacement < INT32_MIN || displacement > INT32_MAX) return -1;
    uint32_t encoded = (uint32_t)(int32_t)displacement;
    memcpy(assembler->text.data + fixup->offset, &encoded, 4);
  }
  return 0;
}

static int assemble(struct assembler *assembler, const char *assembly) {
  size_t length = strlen(assembly);
  char *copy = VIX_ELF_MALLOC(length + 1);
  if (!copy) return -1;
  memcpy(copy, assembly, length + 1);
  assembler->section = SEC_TEXT;

  char *line = copy;
  while (line <= copy + length) {
    char *newline = strchr(line, '\n');
    if (newline) *newline = '\0';
    char *text = trim(line);
    if (*text && *text != ';') {
      if (!strcmp(text, "default rel")) {
      } else if (!strcmp(text, "section .text")) assembler->section = SEC_TEXT;
      else if (!strcmp(text, "section .data") ||
               !strcmp(text, "section .rodata")) assembler->section = SEC_RODATA;
      else if (!strncmp(text, "extern ", 7)) {
        int symbol = find_symbol(assembler, trim(text + 7), 1);
        if (symbol < 0) assembler->failed = 1;
        else assembler->symbols[symbol].global = 1;
      } else if (!strncmp(text, "global ", 7)) {
        int symbol = find_symbol(assembler, trim(text + 7), 1);
        if (symbol < 0) assembler->failed = 1;
        else {
          assembler->symbols[symbol].global = 1;
          assembler->symbols[symbol].function = 1;
        }
      } else if (assembler->section == SEC_RODATA) {
        if (parse_data(assembler, text)) assembler->failed = 1;
      } else {
        size_t text_length = strlen(text);
        if (text_length && text[text_length - 1] == ':') {
          text[text_length - 1] = '\0';
          if (define_symbol(assembler, trim(text))) assembler->failed = 1;
        } else if (emit_instruction(assembler, text)) assembler->failed = 1;
      }
    }
    if (assembler->failed || !newline) break;
    line = newline + 1;
  }
  VIX_ELF_FREE(copy);
  if (assembler->failed) return -1;

  if (resolve_fixups(assembler)) return -1;
  for (int index = 0; index < assembler->symbol_count; index++)
    if (!assembler->symbols[index].defined && !assembler->symbols[index].global)
      return -1;
  return 0;
}

static size_t align_up(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

static int append_string(struct byte_buffer *table, const char *text,
                         uint32_t *offset) {
  if (table->size > UINT32_MAX) return -1;
  *offset = (uint32_t)table->size;
  return buffer_bytes(table, text, strlen(text) + 1);
}

static int write_padding(FILE *file, size_t *position, size_t target) {
  static const unsigned char zeros[16];
  while (*position < target) {
    size_t count = target - *position;
    if (count > sizeof(zeros)) count = sizeof(zeros);
    if (VIX_ELF_FWRITE(zeros, 1, count, file) != count) return -1;
    *position += count;
  }
  return 0;
}

static int write_block(FILE *file, size_t *position, const void *data,
                       size_t size) {
  if (size && VIX_ELF_FWRITE(data, 1, size, file) != size) return -1;
  *position += size;
  return 0;
}

static int write_object(struct assembler *assembler, const char *path) {
  struct byte_buffer strtab = {0}, shstrtab = {0};
  struct elf64_symbol symbols[VIX_ELF_MAX_SYMBOLS + 1];
  struct elf64_rela relas[VIX_ELF_MAX_RELOCS];
  struct elf64_section sections[SEC_COUNT];
  struct elf64_header header;
  uint32_t section_names[SEC_COUNT] = {0};
  int symbol_count = 1;
  int local_count = 0;
  memset(symbols, 0, sizeof(symbols));
  memset(sections, 0, sizeof(sections));
  memset(&header, 0, sizeof(header));
  if (buffer_u8(&strtab, 0) || buffer_u8(&shstrtab, 0)) goto fail;

  for (int pass = 0; pass < 2; pass++) {
    for (int index = 0; index < assembler->symbol_count; index++) {
      struct assembler_symbol *source = &assembler->symbols[index];
      if ((pass == 0) != (source->global == 0)) continue;
      struct elf64_symbol *destination = &symbols[symbol_count];
      if (append_string(&strtab, source->name, &destination->name)) goto fail;
      destination->info = (unsigned char)(((source->global ? STB_GLOBAL : STB_LOCAL) << 4) |
          (source->function ? STT_FUNC :
           source->section == SEC_RODATA ? STT_OBJECT : STT_NOTYPE));
      destination->shndx = source->defined ? (uint16_t)source->section : 0;
      destination->value = source->value;
      source->output_index = symbol_count++;
      if (!source->global) local_count++;
    }
  }
  for (int index = 0; index < assembler->relocation_count; index++) {
    struct object_relocation *source = &assembler->relocations[index];
    if (source->symbol < 0 || source->symbol >= assembler->symbol_count ||
        source->offset > assembler->text.size ||
        assembler->text.size - source->offset < 4 ||
        (source->type != R_X86_64_PC32 && source->type != R_X86_64_PLT32))
      goto fail;
    struct assembler_symbol *symbol = &assembler->symbols[source->symbol];
    relas[index].offset = source->offset;
    relas[index].info = ((uint64_t)symbol->output_index << 32) | source->type;
    relas[index].addend = source->addend;
  }

  const char *names[SEC_COUNT] = {"", ".text", ".rela.text", ".rodata",
                                  ".data", ".symtab", ".strtab", ".shstrtab"};
  for (int index = 1; index < SEC_COUNT; index++)
    if (append_string(&shstrtab, names[index], &section_names[index])) goto fail;

  size_t position = sizeof(header);
  size_t text_offset = align_up(position, 16); position = text_offset + assembler->text.size;
  size_t rela_offset = align_up(position, 8); position = rela_offset + (size_t)assembler->relocation_count * sizeof(relas[0]);
  size_t rodata_offset = align_up(position, 8); position = rodata_offset + assembler->rodata.size;
  size_t data_offset = align_up(position, 8);
  size_t symtab_offset = align_up(position, 8); position = symtab_offset + (size_t)symbol_count * sizeof(symbols[0]);
  size_t strtab_offset = position; position += strtab.size;
  size_t shstrtab_offset = position; position += shstrtab.size;
  size_t shoff = align_up(position, 8);
  if (shoff > VIX_ELF_MAX_OUTPUT - sizeof(sections)) goto fail;

  memcpy(header.ident, "\177ELF\2\1\1", 7);
  header.type = 1;
  header.machine = 62;
  header.version = 1;
  header.shoff = shoff;
  header.ehsize = sizeof(header);
  header.shentsize = sizeof(sections[0]);
  header.shnum = SEC_COUNT;
  header.shstrndx = SEC_SHSTRTAB;

#define SECTION(INDEX, TYPE, FLAGS, OFFSET, SIZE, LINK, INFO, ALIGN, ENTSIZE) \
  sections[INDEX] = (struct elf64_section){section_names[INDEX], TYPE, FLAGS, 0, \
      OFFSET, SIZE, LINK, INFO, ALIGN, ENTSIZE}
  SECTION(SEC_TEXT, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, text_offset,
          assembler->text.size, 0, 0, 16, 0);
  SECTION(SEC_RELA_TEXT, SHT_RELA, 0, rela_offset,
          (size_t)assembler->relocation_count * sizeof(relas[0]), SEC_SYMTAB,
          SEC_TEXT, 8, sizeof(relas[0]));
  SECTION(SEC_RODATA, SHT_PROGBITS, SHF_ALLOC, rodata_offset,
          assembler->rodata.size, 0, 0, 8, 0);
  SECTION(SEC_DATA, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, data_offset, 0, 0, 0, 8, 0);
  SECTION(SEC_SYMTAB, SHT_SYMTAB, 0, symtab_offset,
          (size_t)symbol_count * sizeof(symbols[0]), SEC_STRTAB,
          (uint32_t)(local_count + 1), 8, sizeof(symbols[0]));
  SECTION(SEC_STRTAB, SHT_STRTAB, 0, strtab_offset, strtab.size, 0, 0, 1, 0);
  SECTION(SEC_SHSTRTAB, SHT_STRTAB, 0, shstrtab_offset, shstrtab.size, 0, 0, 1, 0);
#undef SECTION

  FILE *file = VIX_ELF_FOPEN(path, "wb");
  if (!file) goto fail;
  size_t written = 0;
  int result = write_block(file, &written, &header, sizeof(header)) ||
      write_padding(file, &written, text_offset) ||
      write_block(file, &written, assembler->text.data, assembler->text.size) ||
      write_padding(file, &written, rela_offset) ||
      write_block(file, &written, relas,
                  (size_t)assembler->relocation_count * sizeof(relas[0])) ||
      write_padding(file, &written, rodata_offset) ||
      write_block(file, &written, assembler->rodata.data, assembler->rodata.size) ||
      write_padding(file, &written, symtab_offset) ||
      write_block(file, &written, symbols, (size_t)symbol_count * sizeof(symbols[0])) ||
      write_block(file, &written, strtab.data, strtab.size) ||
      write_block(file, &written, shstrtab.data, shstrtab.size) ||
      write_padding(file, &written, shoff) ||
      write_block(file, &written, sections, sizeof(sections));
  if (VIX_ELF_FCLOSE(file) != 0) result = 1;
  if (result) VIX_ELF_REMOVE(path);
  VIX_ELF_FREE(strtab.data);
  VIX_ELF_FREE(shstrtab.data);
  return result ? -1 : 0;

fail:
  VIX_ELF_FREE(strtab.data);
  VIX_ELF_FREE(shstrtab.data);
  return -1;
}

int vix_elf_write_object(const char *assembly, const char *path) {
  struct assembler assembler;
  if (!assembly || !path || !*path) {
    errno = EINVAL;
    return -1;
  }
  memset(&assembler, 0, sizeof(assembler));
  int result = assemble(&assembler, assembly) || write_object(&assembler, path);
  VIX_ELF_FREE(assembler.text.data);
  VIX_ELF_FREE(assembler.rodata.data);
  if (result) {
    VIX_ELF_REMOVE(path);
    errno = ENOEXEC;
    return -1;
  }
  return 0;
}
