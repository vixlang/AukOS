#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MODEL_MAX_VARS 256
#define MODEL_MAX_FUNCTIONS 128
#define MODEL_MAX_STRUCTS 128
#define MODEL_MAX_FIELDS 32
#define MODEL_MAX_PARAMS 16
#define MODEL_NAME_SIZE 64
#define MODEL_TYPE_SIZE 64

struct model_var {
  char name[MODEL_NAME_SIZE];
  char type[MODEL_TYPE_SIZE];
};

struct model_function {
  char name[MODEL_NAME_SIZE];
  char return_type[MODEL_TYPE_SIZE];
  char param_types[MODEL_MAX_PARAMS][MODEL_TYPE_SIZE];
  int param_count;
  int is_var_arg;
};

struct model_struct {
  char name[MODEL_NAME_SIZE];
  char field_names[MODEL_MAX_FIELDS][MODEL_NAME_SIZE];
  char field_types[MODEL_MAX_FIELDS][MODEL_TYPE_SIZE];
  int field_count;
};

static struct model_var variables[MODEL_MAX_VARS];
static struct model_function functions[MODEL_MAX_FUNCTIONS];
static struct model_struct structures[MODEL_MAX_STRUCTS];
static int variable_count;
static int function_count;
static int structure_count;

static void copy_text(char *destination, size_t capacity, const char *source) {
  size_t index = 0;
  if (!source) source = "";
  while (index + 1 < capacity && source[index]) {
    destination[index] = source[index];
    index++;
  }
  destination[index] = '\0';
}

int vix_asm_write_line(void *file, const char *text) {
  if (!file || !text || fputs(text, (FILE *)file) == EOF) return -1;
  return fputc('\n', (FILE *)file) == EOF ? -1 : 0;
}

const char *vix_diag_red(void) { return "\033[31m"; }
const char *vix_diag_yellow(void) { return "\033[33m"; }
const char *vix_diag_reset(void) { return "\033[0m"; }
int vix_diag_strlen(const char *text) { return text ? (int)strlen(text) : 0; }
void vix_print_stderr(const char *text) { if (text) fputs(text, stderr); }

char *vix_driver_create_temp_object(void) {
  static const char pattern[] = "/tmp/vixc-object-XXXXXX";
  char *path = malloc(sizeof(pattern));
  if (!path) return NULL;
  memcpy(path, pattern, sizeof(pattern));
  int descriptor = mkstemp(path);
  if (descriptor < 0) {
    free(path);
    return NULL;
  }
  if (close(descriptor)) {
    int saved_errno = errno;
    unlink(path);
    free(path);
    errno = saved_errno;
    return NULL;
  }
  return path;
}

int vix_driver_discard_temp_object(char *path) {
  if (!path) {
    errno = EINVAL;
    return -1;
  }
  int result = unlink(path);
  if (result && errno == ENOENT) result = 0;
  int saved_errno = errno;
  free(path);
  errno = saved_errno;
  return result;
}

const char *vix_driver_runtime_object(void) {
  const char *override = getenv("VIXC_RUNTIME_OBJECT");
  return override && *override ? override : "/lib/vix/runtime.o";
}

char *vix_join_lines(char **lines) {
  int count = lines ? *(int *)((char *)lines - 8) : 0;
  size_t size = 1;
  for (int index = 0; index < count; index++)
    size += (lines[index] ? strlen(lines[index]) : 0) + 1;
  char *output = malloc(size);
  if (!output) return NULL;
  size_t offset = 0;
  for (int index = 0; index < count; index++) {
    size_t length = lines[index] ? strlen(lines[index]) : 0;
    if (length) memcpy(output + offset, lines[index], length);
    offset += length;
    output[offset++] = '\n';
  }
  output[offset] = '\0';
  return output;
}

char *vix_substr(const char *text, int start, int end) {
  int length = text ? (int)strlen(text) : 0;
  if (start < 0) start = 0;
  if (start > length) start = length;
  if (end < start) end = start;
  if (end > length) end = length;
  char *output = malloc((size_t)(end - start) + 1);
  if (!output) return NULL;
  memcpy(output, text + start, (size_t)(end - start));
  output[end - start] = '\0';
  return output;
}

char *vix_trim_ascii(const char *text) {
  int start = 0;
  int end = text ? (int)strlen(text) : 0;
  while (start < end && (text[start] == ' ' || text[start] == '\t' ||
                         text[start] == '\r' || text[start] == '\n')) start++;
  while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' ||
                         text[end - 1] == '\r' || text[end - 1] == '\n')) end--;
  return vix_substr(text ? text : "", start, end);
}

char *vix_clean_symbol_name(const char *name) {
  int length = name ? (int)strlen(name) : 0;
  int start = length && (name[0] == '%' || name[0] == '@');
  char *output = malloc((size_t)(length - start) + 1);
  int offset = 0;
  if (!output) return NULL;
  for (int index = start; index < length; index++) {
    char value = name[index];
    output[offset++] = ((value >= 'a' && value <= 'z') ||
                        (value >= 'A' && value <= 'Z') ||
                        (value >= '0' && value <= '9') || value == '_')
                           ? value : '_';
  }
  output[offset] = '\0';
  return output;
}

void vix_reset_vars(void) { variable_count = 0; }
void vix_set_var_type(const char *name, const char *type) {
  for (int index = variable_count - 1; index >= 0; index--) {
    if (!strcmp(variables[index].name, name)) {
      copy_text(variables[index].type, sizeof(variables[index].type), type);
      return;
    }
  }
  if (variable_count < MODEL_MAX_VARS) {
    copy_text(variables[variable_count].name, sizeof(variables[0].name), name);
    copy_text(variables[variable_count].type, sizeof(variables[0].type), type);
    variable_count++;
  }
}
const char *vix_get_var_type(const char *name) {
  for (int index = variable_count - 1; index >= 0; index--)
    if (!strcmp(variables[index].name, name)) return variables[index].type;
  return "i32";
}

void vix_reset_function_sigs(void) { function_count = 0; }
void vix_register_function_sig_vararg(const char *name, const char *return_type,
                                      const char **param_types, int param_count,
                                      int is_var_arg) {
  int slot = -1;
  for (int index = 0; index < function_count; index++)
    if (!strcmp(functions[index].name, name)) slot = index;
  if (slot < 0) {
    if (function_count == MODEL_MAX_FUNCTIONS) return;
    slot = function_count++;
  }
  if (param_count > MODEL_MAX_PARAMS) param_count = MODEL_MAX_PARAMS;
  copy_text(functions[slot].name, sizeof(functions[slot].name), name);
  copy_text(functions[slot].return_type, sizeof(functions[slot].return_type),
            return_type);
  functions[slot].param_count = param_count;
  functions[slot].is_var_arg = is_var_arg;
  for (int index = 0; index < param_count; index++)
    copy_text(functions[slot].param_types[index], MODEL_TYPE_SIZE,
              param_types[index]);
}
const char *vix_get_function_return_type(const char *name) {
  for (int index = function_count - 1; index >= 0; index--)
    if (!strcmp(functions[index].name, name)) return functions[index].return_type;
  return "i32";
}
const char *vix_get_function_param_type(const char *name, int parameter) {
  for (int index = function_count - 1; index >= 0; index--)
    if (!strcmp(functions[index].name, name))
      return parameter >= 0 && parameter < functions[index].param_count
                 ? functions[index].param_types[parameter] : "i32";
  return "i32";
}
int vix_get_function_param_count(const char *name) {
  for (int index = function_count - 1; index >= 0; index--)
    if (!strcmp(functions[index].name, name)) return functions[index].param_count;
  return 0;
}
int vix_get_function_is_var_arg(const char *name) {
  for (int index = function_count - 1; index >= 0; index--)
    if (!strcmp(functions[index].name, name)) return functions[index].is_var_arg;
  return 0;
}

static int find_struct(const char *name) {
  for (int index = structure_count - 1; index >= 0; index--)
    if (!strcmp(structures[index].name, name)) return index;
  return -1;
}
void vix_reset_struct_sigs(void) { structure_count = 0; }
void vix_declare_struct_sig(const char *name) {
  if (find_struct(name) >= 0 || structure_count == MODEL_MAX_STRUCTS) return;
  copy_text(structures[structure_count].name,
            sizeof(structures[structure_count].name), name);
  structures[structure_count].field_count = 0;
  structure_count++;
}
void vix_register_struct_sig(const char *name, const char **field_names,
                             const char **field_types, int field_count) {
  vix_declare_struct_sig(name);
  int slot = find_struct(name);
  if (slot < 0) return;
  if (field_count > MODEL_MAX_FIELDS) field_count = MODEL_MAX_FIELDS;
  structures[slot].field_count = field_count;
  for (int index = 0; index < field_count; index++) {
    copy_text(structures[slot].field_names[index], MODEL_NAME_SIZE,
              field_names[index]);
    copy_text(structures[slot].field_types[index], MODEL_TYPE_SIZE,
              field_types[index]);
  }
}
int vix_is_struct_type(const char *name) { return find_struct(name) >= 0; }
int vix_get_struct_field_index(const char *struct_name, const char *field_name) {
  if (!strncmp(struct_name, "ptr:", 4)) struct_name += 4;
  int slot = find_struct(struct_name);
  if (slot < 0) return -1;
  for (int index = 0; index < structures[slot].field_count; index++)
    if (!strcmp(structures[slot].field_names[index], field_name)) return index;
  return -1;
}
const char *vix_get_struct_field_type(const char *struct_name,
                                      const char *field_name) {
  if (!strncmp(struct_name, "ptr:", 4)) struct_name += 4;
  int slot = find_struct(struct_name);
  if (slot < 0) return "unknown";
  int field = vix_get_struct_field_index(struct_name, field_name);
  return field >= 0 ? structures[slot].field_types[field] : "unknown";
}
