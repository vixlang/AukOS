#include <stddef.h>

extern int main(void);

static volatile int host_state = 7;

int printf(const char *format, ...) { (void)format; return 0; }
void *calloc(size_t count, size_t size) { (void)count; (void)size; return 0; }
int strcmp(const char *left, const char *right) {
  while (*left && *left == *right) { left++; right++; }
  return (unsigned char)*left - (unsigned char)*right;
}
size_t strlen(const char *text) {
  size_t length = 0;
  while (text[length]) length++;
  return length;
}
int vix_array_len(void *array) { (void)array; return 0; }
void *vix_array_push_i32(void *array, int value) {
  (void)value;
  return array;
}
void *vix_array_push_ptr(void *array, void *value) {
  (void)value;
  return array;
}
void *vix_array_push_bytes(void *array, void *value, size_t size) {
  (void)value;
  (void)size;
  return array;
}
void *vix_string_concat(const char *left, const char *right) {
  (void)left;
  (void)right;
  return 0;
}

void _start(void) {
  long code = (long)main() + host_state - 7;
  __asm__ volatile("syscall" : : "a"(60L), "D"(code) : "rcx", "r11", "memory");
  __builtin_unreachable();
}
