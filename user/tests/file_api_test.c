#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define LARGE_SIZE (256u * 1024u)

static int fail(const char *step)
{
    printf("[file_api_test] FAIL %s\n", step);
    return 1;
}

static int read_exact_at(int fd, off_t offset, void *buffer, size_t size)
{
    return lseek(fd, offset, SEEK_SET) == offset &&
           read(fd, buffer, size) == (ssize_t)size ? 0 : -1;
}

static int generated_child(int argc, char **argv)
{
    char cwd[256];

    if (argc != 2 || strcmp(argv[1], "--generated-child") != 0 ||
        !getenv("AUKOS_GENERATED") ||
        strcmp(getenv("AUKOS_GENERATED"), "yes") != 0 ||
        !getcwd(cwd, sizeof(cwd)) || strcmp(cwd, "/work") != 0) {
        return fail("generated argv/env/cwd");
    }
    puts("[generated_exec_test] PASS");
    return 0;
}

static int test_flags_and_offsets(void)
{
    char buffer[32];
    int fd;
    int other;

    fd = open("/work/flags", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0 || write(fd, "keep", 4u) != 4 || close(fd) != 0) {
        return fail("create flags");
    }
    if (open("/work/flags", O_CREAT | O_EXCL | O_RDWR | O_TRUNC, 0644) >= 0) {
        return fail("O_EXCL collision");
    }
    fd = open("/work/flags", O_RDONLY);
    if (fd < 0 || write(fd, "x", 1u) != -1 ||
        read(fd, buffer, 4u) != 4 || memcmp(buffer, "keep", 4u) != 0 ||
        close(fd) != 0) {
        return fail("read-only access");
    }
    fd = open("/work/flags", O_WRONLY);
    if (fd < 0 || read(fd, buffer, 1u) != -1 || close(fd) != 0) {
        return fail("write-only access");
    }
    fd = open("/bin/file_api_test", O_RDWR);
    if (fd < 0 || write(fd, "x", 1u) != -1 || close(fd) != 0 ||
        truncate("/bin/file_api_test", 0) != -1) {
        return fail("static backing mutation");
    }

    fd = open("/work/append", O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, 0644);
    other = open("/work/append", O_WRONLY | O_APPEND);
    if (fd < 0 || other < 0 || write(fd, "one", 3u) != 3 ||
        write(other, "two", 3u) != 3 || close(fd) != 0 || close(other) != 0) {
        return fail("append write");
    }
    fd = open("/work/append", O_RDONLY);
    if (fd < 0 || read(fd, buffer, 6u) != 6 ||
        memcmp(buffer, "onetwo", 6u) != 0 || close(fd) != 0) {
        return fail("append contents");
    }

    fd = open("/work/hole", O_CREAT | O_RDWR | O_TRUNC, 0644);
    memset(buffer, 0x7f, sizeof(buffer));
    if (fd < 0 || lseek(fd, 8192, SEEK_SET) != 8192 ||
        write(fd, "end", 3u) != 3 || read_exact_at(fd, 8184, buffer, 8u) != 0) {
        return fail("seek hole");
    }
    for (int index = 0; index < 8; index++) {
        if (buffer[index] != 0) {
            return fail("hole zero-fill");
        }
    }
    if (close(fd) != 0) {
        return fail("hole close");
    }
    return 0;
}

static int test_truncate(void)
{
    char buffer[16];
    int fd = open("/work/truncate", O_CREAT | O_RDWR | O_TRUNC, 0644);

    if (fd < 0 || lseek(fd, 4094, SEEK_SET) != 4094 ||
        write(fd, "boundary", 8u) != 8 || lseek(fd, 5000, SEEK_SET) != 5000 ||
        ftruncate(fd, 4097) != 0 || lseek(fd, 0, SEEK_CUR) != 5000 ||
        ftruncate(fd, 8200) != 0 || lseek(fd, 0, SEEK_CUR) != 5000 ||
        read_exact_at(fd, 4097, buffer, sizeof(buffer)) != 0) {
        return fail("ftruncate shrink/grow/offset");
    }
    for (size_t index = 0; index < sizeof(buffer); index++) {
        if (buffer[index] != 0) {
            return fail("truncate grow zero-fill");
        }
    }
    if (close(fd) != 0 || truncate("/work/truncate", 2) != 0 ||
        truncate("/work/truncate", -1) != -1) {
        return fail("truncate path");
    }
    return 0;
}

static int test_namespace_lifetime(void)
{
    char buffer[8];
    int old_fd;
    int new_fd;

    old_fd = open("/work/lifetime", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (old_fd < 0 || write(old_fd, "old", 3u) != 3 ||
        unlink("/work/lifetime") != 0) {
        return fail("unlink open setup");
    }
    new_fd = open("/work/lifetime", O_CREAT | O_RDWR | O_EXCL, 0644);
    if (new_fd < 0 || write(new_fd, "new", 3u) != 3 ||
        read_exact_at(old_fd, 0, buffer, 3u) != 0 ||
        memcmp(buffer, "old", 3u) != 0 ||
        read_exact_at(new_fd, 0, buffer, 3u) != 0 ||
        memcmp(buffer, "new", 3u) != 0 || close(old_fd) != 0 ||
        close(new_fd) != 0) {
        return fail("unlink recreate lifetime");
    }

    old_fd = open("/work/source", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (old_fd < 0 || write(old_fd, "source", 6u) != 6 || close(old_fd) != 0 ||
        rename("/work/source", "/work/moved") != 0 ||
        rename("/work/moved", "/work/moved") != 0 ||
        rename("/work/missing", "/work/moved") != -1) {
        return fail("rename empty/same/missing");
    }
    old_fd = open("/work/target", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (old_fd < 0 || write(old_fd, "target", 6u) != 6 ||
        rename("/work/moved", "/work/target") != 0) {
        return fail("rename overwrite setup");
    }
    new_fd = open("/work/target", O_RDONLY);
    if (new_fd < 0 || read_exact_at(old_fd, 0, buffer, 6u) != 0 ||
        memcmp(buffer, "target", 6u) != 0 ||
        read_exact_at(new_fd, 0, buffer, 6u) != 0 ||
        memcmp(buffer, "source", 6u) != 0 || close(old_fd) != 0 ||
        close(new_fd) != 0) {
        return fail("rename overwritten fd lifetime");
    }
    return 0;
}

static int test_shared_offsets(void)
{
    char byte;
    int fd = open("/work/shared", O_CREAT | O_RDWR | O_TRUNC, 0644);
    int duplicate;
    pid_t child;
    int status = 1;

    if (fd < 0 || write(fd, "abcdef", 6u) != 6 || lseek(fd, 0, SEEK_SET) != 0 ||
        unlink("/work/shared") != 0 ||
        (duplicate = dup(fd)) < 0 || read(fd, &byte, 1u) != 1 || byte != 'a' ||
        read(duplicate, &byte, 1u) != 1 || byte != 'b' || close(duplicate) != 0 ||
        lseek(fd, 0, SEEK_SET) != 0) {
        return fail("dup shared offset");
    }
    child = fork();
    if (child == 0) {
        _exit(read(fd, &byte, 1u) == 1 && byte == 'a' ? 0 : 2);
    }
    if (child < 0 || waitpid(child, &status, 0) != child || status != 0 ||
        read(fd, &byte, 1u) != 1 || byte != 'b' || close(fd) != 0) {
        return fail("fork shared offset");
    }
    return 0;
}

static int test_fsync_and_mkstemp(void)
{
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char first[] = "/work/tempXXXXXX";
    char second[] = "/work/tempXXXXXX";
    char collision[] = "/work/collisionXXXXXX";
    char collision_candidate[] = "/work/collisionXXXXXX";
    char invalid[] = "/work/badXXXXX";
    struct stat status;
    int pipefd[2];
    int fd = open("/work/sync", O_CREAT | O_RDWR | O_TRUNC, 0644);
    int first_fd;
    int second_fd;
    int collision_fd;
    int occupied_fd;
    uint64_t random_value = 0x41756b4f53524e47ull;
    uint64_t name_value;

    if (fd < 0 || fsync(fd) != 0 || close(fd) != 0 || fsync(fd) != -1 ||
        pipe(pipefd) != 0 || fsync(pipefd[0]) != -1 ||
        ftruncate(pipefd[0], 0) != -1 || ftruncate(1, 0) != -1 ||
        close(pipefd[0]) != 0 || close(pipefd[1]) != 0) {
        return fail("fsync validation");
    }
    random_value = random_value * 6364136223846793005ull + 1u;
    name_value = random_value ^ 1u;
    for (size_t index = 0; index < 6u; index++) {
        collision_candidate[15u + index] = alphabet[name_value % 62u];
        name_value = name_value / 62u + 17u;
    }
    occupied_fd = open(collision_candidate, O_CREAT | O_EXCL | O_RDWR, 0600);
    collision_fd = mkstemp(collision);
    if (occupied_fd < 0 || collision_fd < 0 ||
        strcmp(collision, collision_candidate) == 0 || close(occupied_fd) != 0 ||
        close(collision_fd) != 0 || unlink(collision_candidate) != 0 ||
        unlink(collision) != 0) {
        return fail("mkstemp collision retry");
    }
    first_fd = mkstemp(first);
    second_fd = mkstemp(second);
    if (first_fd < 0 || second_fd < 0 || strcmp(first, second) == 0 ||
        strcmp(first, "/work/tempXXXXXX") == 0 ||
        fstat(first_fd, &status) != 0 || (status.st_mode & 0777u) != 0600u ||
        mkstemp(invalid) != -1 || strcmp(invalid, "/work/badXXXXX") != 0 ||
        close(first_fd) != 0 || close(second_fd) != 0 ||
        unlink(first) != 0 || unlink(second) != 0) {
        return fail("mkstemp unique/template");
    }
    return 0;
}

static int test_reuse_and_large_file(void)
{
    char chunk[4096];
    int fd;

    memset(chunk, 0x5a, sizeof(chunk));
    for (int iteration = 0; iteration < 128; iteration++) {
        fd = open("/work/reuse", O_CREAT | O_EXCL | O_RDWR, 0644);
        if (fd < 0 || write(fd, &iteration, sizeof(iteration)) !=
            (ssize_t)sizeof(iteration) ||
            rename("/work/reuse", "/work/reused") != 0 ||
            unlink("/work/reused") != 0 || close(fd) != 0) {
            return fail("128-round namespace reuse");
        }
    }
    fd = open("/work/large", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        return fail("large open");
    }
    for (int cycle = 0; cycle < 3; cycle++) {
        if (ftruncate(fd, 0) != 0 || lseek(fd, 0, SEEK_SET) != 0) {
            return fail("large shrink");
        }
        for (size_t offset = 0; offset < LARGE_SIZE; offset += sizeof(chunk)) {
            if (write(fd, chunk, sizeof(chunk)) != (ssize_t)sizeof(chunk)) {
                return fail("large grow");
            }
        }
    }
    if (close(fd) != 0 || unlink("/work/large") != 0) {
        return fail("large reclaim");
    }
    return 0;
}

static int generated_exec(void)
{
    char buffer[4096];
    char *arguments[] = {"generated.elf", "--generated-child", 0};
    int source = open("/bin/file_api_test", O_RDONLY);
    int destination = open("/work/generated.elf",
                           O_CREAT | O_EXCL | O_WRONLY, 0755);
    pid_t child;
    int status = 1;
    ssize_t count;

    if (source < 0 || destination < 0) {
        return fail("generated open");
    }
    while ((count = read(source, buffer, sizeof(buffer))) > 0) {
        if (write(destination, buffer, (size_t)count) != count) {
            return fail("generated copy");
        }
    }
    if (count != 0 || fsync(destination) != 0 || close(source) != 0 ||
        close(destination) != 0 || setenv("AUKOS_GENERATED", "yes", 1) != 0 ||
        chdir("/work") != 0) {
        return fail("generated finalize");
    }
    child = fork();
    if (child == 0) {
        execv("/work/generated.elf", arguments);
        _exit(127);
    }
    if (child < 0 || waitpid(child, &status, 0) != child || status != 0 ||
        unlink("/work/generated.elf") != 0) {
        return fail("generated fork/exec/wait");
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--generated-child") == 0) {
        return generated_child(argc, argv);
    }
    /* Recover test-owned names if a previous QEMU run was stopped mid-test. */
    (void)unlink("/work/reuse");
    (void)unlink("/work/reused");
    (void)unlink("/work/generated.elf");
    if (test_flags_and_offsets() != 0 || test_truncate() != 0 ||
        test_namespace_lifetime() != 0 || test_shared_offsets() != 0 ||
        test_fsync_and_mkstemp() != 0 || test_reuse_and_large_file() != 0 ||
        generated_exec() != 0) {
        return 1;
    }
    puts("[file_api_test] PASS");
    return 0;
}
