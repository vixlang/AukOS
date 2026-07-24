#include <assert.h>
#include <stdio.h>
#include <string.h>

int aukos_test_errno;

int main(void)
{
    char buffer[64];
    const char *long_text = "%s";

    assert(snprintf(buffer, sizeof(buffer), "%*ld", 0, 23L) == 2);
    assert(strcmp(buffer, "23") == 0);
    assert(snprintf(buffer, sizeof(buffer), "%*.*s", 0, -1, "AukOS") == 5);
    assert(strcmp(buffer, "AukOS") == 0);
    assert(snprintf(buffer, sizeof(buffer), "%-7.3s", "argument") == 7);
    assert(strcmp(buffer, "arg    ") == 0);
    assert(snprintf(buffer, sizeof(buffer), "%05d", -12) == 5);
    assert(strcmp(buffer, "-0012") == 0);
    assert(snprintf(buffer, 5, long_text, "AukOS") == 5);
    assert(strcmp(buffer, "AukO") == 0);

    return 0;
}
