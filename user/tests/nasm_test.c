#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_DIR "/work/nasm-test"
#define CAPTURE_SIZE 4096u

static const char hello_source[] =
    "default rel\n"
    "global main\n"
    "extern aukos_vix_write_str\n"
    "section .rodata\n"
    "message: db \"[nasm_generated_exec_test] PASS\", 10, 0\n"
    "section .text\n"
    "main:\n"
    "    push rbp\n"
    "    mov rbp, rsp\n"
    "    lea rdi, [rel message]\n"
    "    call aukos_vix_write_str wrt ..plt\n"
    "    xor eax, eax\n"
    "    pop rbp\n"
    "    ret\n";

static int last_status;

static int fail(const char *step)
{
    printf("[nasm_test] FAIL %s status=%d\n", step, last_status);
    return 1;
}

static int write_file(const char *path, const char *data)
{
    size_t size = strlen(data);
    size_t offset = 0;
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    if (fd < 0) return -1;
    while (offset < size) {
        ssize_t count = write(fd, data + offset, size - offset);
        if (count <= 0) { close(fd); return -1; }
        offset += (size_t)count;
    }
    return fsync(fd) == 0 && close(fd) == 0 ? 0 : -1;
}

static int file_equals(const char *path, const char *expected)
{
    char buffer[512];
    size_t size = strlen(expected);
    int fd = open(path, O_RDONLY);
    ssize_t count;

    if (fd < 0 || size > sizeof(buffer)) return 0;
    count = read(fd, buffer, sizeof(buffer));
    return close(fd) == 0 && count == (ssize_t)size &&
           memcmp(buffer, expected, size) == 0;
}

static int run_captured(const char *program, char *const argv[], int success,
                        char *capture, size_t capacity)
{
    extern char **environ;
    int pipefd[2];
    int status = -1;
    size_t used = 0;
    pid_t child;

    if (pipe(pipefd) != 0) return -1;
    child = fork();
    if (child == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], 1) < 0 || dup2(pipefd[1], 2) < 0) _exit(126);
        close(pipefd[1]);
        execve(program, argv, environ);
        _exit(127);
    }
    close(pipefd[1]);
    if (child < 0) { close(pipefd[0]); return -1; }
    for (;;) {
        char discard[256];
        char *destination = used + 1u < capacity ? capture + used : discard;
        size_t available = used + 1u < capacity ? capacity - used - 1u : sizeof(discard);
        ssize_t count = read(pipefd[0], destination, available);
        if (count < 0) { close(pipefd[0]); return -1; }
        if (count == 0) break;
        if (destination != discard) used += (size_t)count;
    }
    close(pipefd[0]);
    if (waitpid(child, &status, 0) != child) return -1;
    last_status = status;
    if (capacity) capture[used] = '\0';
    return success ? (status == 0 ? 0 : -1) :
                     (status != 0 && used != 0u ? 0 : -1);
}

static int count_temporaries(void)
{
    DIR *directory = opendir(TEST_DIR);
    struct dirent *entry;
    int count = 0;

    if (!directory) return -1;
    while ((entry = readdir(directory)) != 0) {
        if (strstr(entry->d_name, ".nasm-tmp-")) count++;
    }
    return closedir(directory) == 0 ? count : -1;
}

static int audit_object(const char *path)
{
    unsigned char header[20];
    int fd = open(path, O_RDONLY);

    if (fd < 0 || read(fd, header, sizeof(header)) != (ssize_t)sizeof(header) ||
        fsync(fd) != 0 || close(fd) != 0) return -1;
    return memcmp(header, "\177ELF\2\1\1", 7) == 0 &&
           header[16] == 1 && header[17] == 0 &&
           header[18] == 62 && header[19] == 0 ? 0 : -1;
}

static int fsync_path(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    return fsync(fd) == 0 && close(fd) == 0 ? 0 : -1;
}

int main(void)
{
    char capture[CAPTURE_SIZE];
    struct stat status;
    int baseline;
    char *version[] = {"nasm", "--version", 0};
    char *help[] = {"nasm", "--help", 0};
    char *no_input[] = {"nasm", 0};
    char *missing[] = {"nasm", "-f", "elf64", TEST_DIR "/missing.asm", "-o", TEST_DIR "/missing.o", 0};
    char *bad_format[] = {"nasm", "-f", "bin", TEST_DIR "/hello.asm", "-o", TEST_DIR "/bad-format.o", 0};
    char *missing_output[] = {"nasm", "-f", "elf64", TEST_DIR "/hello.asm", "-o", 0};
    char *duplicate_output[] = {"nasm", "-f", "elf64", TEST_DIR "/hello.asm", "-o", TEST_DIR "/one.o", "-o", TEST_DIR "/two.o", 0};
    char *same_path[] = {"nasm", "-f", "elf64", TEST_DIR "/hello.asm", "-o", TEST_DIR "/hello.asm", 0};
    char *bad_syntax[] = {"nasm", "-f", "elf64", TEST_DIR "/invalid.asm", "-o", TEST_DIR "/preserved.o", 0};

    if (mkdir(TEST_DIR, 0755) != 0 &&
        (stat(TEST_DIR, &status) != 0 || (status.st_mode & S_IFMT) != S_IFDIR))
        return fail("mkdir");
    if (write_file(TEST_DIR "/hello.asm", hello_source) != 0 ||
        write_file(TEST_DIR "/invalid.asm", "this is not an instruction !\n") != 0)
        return fail("sources");

    if (run_captured("/bin/nasm", version, 1, capture, sizeof(capture)) != 0 ||
        !strstr(capture, "NASM version 3.02")) return fail("version");
    if (run_captured("/bin/nasm", help, 1, capture, sizeof(capture)) != 0 ||
        !strstr(capture, "Usage: nasm")) return fail("help");
    if (run_captured("/bin/nasm", no_input, 0, capture, sizeof(capture)) != 0)
        return fail("missing input");
    if (run_captured("/bin/nasm", missing, 0, capture, sizeof(capture)) != 0)
        return fail("invalid input");
    if (run_captured("/bin/nasm", bad_format, 0, capture, sizeof(capture)) != 0)
        return fail("unsupported format");
    if (run_captured("/bin/nasm", missing_output, 0, capture, sizeof(capture)) != 0)
        return fail("missing output argument");
    if (run_captured("/bin/nasm", duplicate_output, 0, capture, sizeof(capture)) != 0)
        return fail("duplicate output");
    if (run_captured("/bin/nasm", same_path, 0, capture, sizeof(capture)) != 0 ||
        !file_equals(TEST_DIR "/hello.asm", hello_source)) return fail("same path");

    if (write_file(TEST_DIR "/preserved.o", "preserve-me") != 0)
        return fail("preservation setup");
    baseline = count_temporaries();
    if (baseline < 0 || run_captured("/bin/nasm", bad_syntax, 0, capture,
                                    sizeof(capture)) != 0 ||
        !file_equals(TEST_DIR "/preserved.o", "preserve-me") ||
        count_temporaries() != baseline) return fail("failure preservation/cleanup");

    if (chdir(TEST_DIR) != 0 || write_file("-dash.asm", hello_source) != 0)
        return fail("relative setup");
    {
        char *dash[] = {"nasm", "-f", "elf64", "-o", "dash.o", "--", "-dash.asm", 0};
        if (run_captured("/bin/nasm", dash, 1, capture, sizeof(capture)) != 0 ||
            audit_object("dash.o") != 0 || unlink("dash.o") != 0 ||
            unlink("-dash.asm") != 0) return fail("option terminator/relative path");
    }
    for (int iteration = 0; iteration < 8; iteration++) {
        char *assemble[] = {"nasm", "-f", "elf64", "hello.asm", "-o", "reuse.o", 0};
        if (run_captured("/bin/nasm", assemble, 1, capture, sizeof(capture)) != 0 ||
            audit_object("reuse.o") != 0 || count_temporaries() != baseline ||
            unlink("reuse.o") != 0) return fail("eight-round reuse");
    }
    {
        char *assemble[] = {"nasm", "-f", "elf64", TEST_DIR "/hello.asm", "-o", TEST_DIR "/hello.o", 0};
        char *link[] = {"vixc", "--link", TEST_DIR "/hello.o", "-o", TEST_DIR "/hello", 0};
        char *execute[] = {"hello", 0};
        if (run_captured("/bin/nasm", assemble, 1, capture, sizeof(capture)) != 0 ||
            audit_object(TEST_DIR "/hello.o") != 0 ||
            run_captured("/bin/vixc", link, 1, capture, sizeof(capture)) != 0 ||
            fsync_path(TEST_DIR "/hello") != 0 ||
            run_captured(TEST_DIR "/hello", execute, 1, capture, sizeof(capture)) != 0 ||
            !strstr(capture, "[nasm_generated_exec_test] PASS"))
            return fail("assemble-link-exec");
        fputs(capture, stdout);
    }
    puts("[nasm_cli_test] PASS");
    puts("[nasm_output_preservation_test] PASS");
    puts("[nasm_temp_cleanup_test] PASS");
    puts("[nasm_reuse_test] PASS 8/8");
    return 0;
}
