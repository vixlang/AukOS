#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *vix_driver_create_temp_object(void);
int vix_driver_discard_temp_object(char *path);

int main(void) {
  char *first = vix_driver_create_temp_object();
  char *second = vix_driver_create_temp_object();
  if (!first || !second || !strcmp(first, second) || access(first, F_OK) ||
      access(second, F_OK)) {
    fputs("freestanding_driver_helper_test: unique temp creation failed\n",
          stderr);
    return 1;
  }
  if (vix_driver_discard_temp_object(first) ||
      vix_driver_discard_temp_object(second)) {
    fputs("freestanding_driver_helper_test: temp cleanup failed\n", stderr);
    return 1;
  }
  puts("freestanding_driver_helper_test: PASS");
  return 0;
}
