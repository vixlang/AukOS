#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <aukos/runtime.h>

#define TEST_SIZE (12u * 1024u)

static int fail(const char *reason)
{
    aukos_vix_write_str("[vix_runtime_test] FAIL: ");
    aukos_vix_write_str(reason);
    aukos_vix_write_str("\n");
    return 1;
}

int main(void)
{
    unsigned char *data = aukos_vix_malloc(TEST_SIZE);
    unsigned char *copy = aukos_vix_calloc(TEST_SIZE, 1u);
    void *first;
    void *reused;
    void *stream;

    if (!data || !copy) return fail("allocate");
    for (size_t index = 0; index < TEST_SIZE; index++) {
        data[index] = (unsigned char)((index * 37u + 11u) & 0xffu);
    }
    stream = aukos_vix_fopen("/work/vix-runtime.tmp", "w+");
    if (!stream) return fail("fopen w+");
    if (aukos_vix_fwrite(data, 1u, TEST_SIZE, stream) != TEST_SIZE)
        return fail("multi-page write");
    if (aukos_vix_fseek(stream, 4093, 0) != 0 ||
        aukos_vix_fread(copy, 1u, 17u, stream) != 17u ||
        memcmp(copy, data + 4093u, 17u) != 0)
        return fail("cross-page seek/read");
    if (aukos_vix_fclose(stream) != 0) return fail("close");

    stream = aukos_vix_fopen("/work/vix-runtime.tmp", "r");
    if (!stream || aukos_vix_fseek(stream, 0, 2) != 0 ||
        aukos_vix_ftell(stream) != (long)TEST_SIZE)
        return fail("reopen/size");
    if (aukos_vix_fseek(stream, 0, 0) != 0 ||
        aukos_vix_fread(copy, 1u, TEST_SIZE, stream) != TEST_SIZE ||
        memcmp(copy, data, TEST_SIZE) != 0)
        return fail("reopen/read");
    if (aukos_vix_fclose(stream) != 0) return fail("reopen close");

    if (aukos_vix_rename("/work/vix-runtime.tmp",
                         "/work/vix-runtime.bin") != 0)
        return fail("rename");
    if (aukos_vix_remove("/work/vix-runtime.bin") != 0)
        return fail("remove");

    first = aukos_vix_malloc(256u);
    if (!first) return fail("reuse allocate");
    aukos_vix_memset(first, 0x5a, 256u);
    aukos_vix_free(first);
    reused = aukos_vix_malloc(128u);
    if (reused != first) return fail("allocator reuse");
    reused = aukos_vix_realloc(reused, 512u);
    if (!reused || ((unsigned char *)reused)[0] != 0x5a)
        return fail("realloc preserve");

    aukos_vix_free(reused);
    aukos_vix_free(copy);
    aukos_vix_free(data);
    if (aukos_vix_write_str("[vix_runtime_test] PASS\n") != 24)
        return fail("marker write");
    return 0;
}
