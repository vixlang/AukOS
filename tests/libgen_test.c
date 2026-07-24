#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char basename_path[] = "/ext4/hello.txt";
    char basename_trailing[] = "/ext4/dir///";
    char dirname_path[] = "/ext4/hello.txt";
    char dirname_relative[] = "ext4/hello.txt";
    char dirname_root[] = "/hello.txt";
    char no_slash[] = "hello.txt";

    assert(strcmp(basename(basename_path), "hello.txt") == 0);
    assert(strcmp(basename(basename_trailing), "dir") == 0);
    assert(strcmp(dirname(dirname_path), "/ext4") == 0);
    assert(strcmp(dirname(dirname_relative), "ext4") == 0);
    assert(strcmp(dirname(dirname_root), "/") == 0);
    assert(strcmp(dirname(no_slash), ".") == 0);

    puts("libgen_test: PASS");
    return 0;
}
