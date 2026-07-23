#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PERSIST_DIR "/work/persist"
#define DATA_SIZE 9000u

static const char hello_source[] =
    "extern \"C\"\n"
    "{\n"
    "    fn aukos_vix_write_str(text: string): i64\n"
    "    fn aukos_vix_streq(left: string, right: string): i32\n"
    "}\n"
    "fn main(argc: i32, argv: &string): i32\n"
    "{\n"
    "    if (argc != 2) { return 2 }\n"
    "    if (aukos_vix_streq(argv[1], \"first\") == 1)\n"
    "    {\n"
    "        aukos_vix_write_str(\"[persistent_vix_exec_first_boot] PASS\\n\")\n"
    "        return 0\n"
    "    }\n"
    "    if (aukos_vix_streq(argv[1], \"second\") == 1)\n"
    "    {\n"
    "        aukos_vix_write_str(\"[persistent_vix_exec_second_boot] PASS\\n\")\n"
    "        return 0\n"
    "    }\n"
    "    return 3\n"
    "}\n";

static int fail(const char *step)
{
    printf("[persistent_work] FAIL %s\n", step);
    return 1;
}

static int write_exact(int fd, const void *buffer, size_t size)
{
    const char *bytes = buffer;
    size_t offset = 0;
    while (offset < size) {
        ssize_t count = write(fd, bytes + offset, size - offset);
        if (count <= 0) return -1;
        offset += (size_t)count;
    }
    return 0;
}

static int read_exact(int fd, void *buffer, size_t size)
{
    char *bytes = buffer;
    size_t offset = 0;
    while (offset < size) {
        ssize_t count = read(fd, bytes + offset, size - offset);
        if (count <= 0) return -1;
        offset += (size_t)count;
    }
    return 0;
}

static int write_file(const char *path, const void *data, size_t size, int mode)
{
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, mode);
    if (fd < 0 || write_exact(fd, data, size) || fsync(fd) || close(fd)) return -1;
    return 0;
}

static int file_equals(const char *path, const void *expected, size_t size)
{
    char buffer[512];
    const char *bytes = expected;
    size_t offset = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    while (offset < size) {
        size_t chunk = size - offset;
        if (chunk > sizeof(buffer)) chunk = sizeof(buffer);
        if (read_exact(fd, buffer, chunk) || memcmp(buffer, bytes + offset, chunk)) {
            close(fd);
            return 0;
        }
        offset += chunk;
    }
    if (read(fd, buffer, 1u) != 0 || close(fd)) return 0;
    return 1;
}

static int path_exists(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    return close(fd) == 0;
}

static int run(char *path, char *const argv[], int expected_success)
{
    pid_t child = fork();
    int status = -1;
    if (child == 0) {
        execv(path, argv);
        _exit(127);
    }
    if (child < 0 || waitpid(child, &status, 0) != child) return -1;
    return expected_success ? (status == 0 ? 0 : -1) : (status != 0 ? 0 : -1);
}

static void fill_data(unsigned char *data)
{
    for (size_t i = 0; i < DATA_SIZE; i++) data[i] = (unsigned char)(i * 29u + 7u);
}

static int execute_generated(const char *phase)
{
    char *arguments[] = {"hello", (char *)phase, 0};
    return run(PERSIST_DIR "/hello", arguments, 1);
}

static int first_boot(void)
{
    unsigned char data[DATA_SIZE], verify[DATA_SIZE];
    struct stat status;
    struct dirent *entry;
    DIR *directory;
    int data_fd, old_fd, new_fd, found_data = 0;
    char *compile[] = {"vixc", PERSIST_DIR "/hello.vix", "-o",
                       PERSIST_DIR "/hello", 0};
    char *bad_compile[] = {"vixc", PERSIST_DIR "/invalid.vix", "-o",
                           PERSIST_DIR "/preserved", 0};

    fill_data(data);
    data_fd = open(PERSIST_DIR "/data.txt", O_CREAT | O_EXCL | O_RDWR, 0644);
    if (data_fd < 0 || write_exact(data_fd, data, sizeof(data)) || fsync(data_fd) ||
        close(data_fd) || (data_fd = open(PERSIST_DIR "/data.txt", O_RDONLY)) < 0 ||
        read_exact(data_fd, verify, sizeof(verify)) || memcmp(data, verify, sizeof(data)) ||
        close(data_fd)) return fail("cross-block data");

    old_fd = open(PERSIST_DIR "/old-name", O_CREAT | O_EXCL | O_RDWR, 0644);
    new_fd = open(PERSIST_DIR "/replacement", O_CREAT | O_EXCL | O_RDWR, 0644);
    if (old_fd < 0 || new_fd < 0 || write_exact(old_fd, "source", 6u) ||
        write_exact(new_fd, "target", 6u) ||
        rename(PERSIST_DIR "/old-name", PERSIST_DIR "/replacement") ||
        lseek(new_fd, 0, SEEK_SET) != 0 || read_exact(new_fd, verify, 6u) ||
        memcmp(verify, "target", 6u) || close(old_fd) || close(new_fd))
        return fail("rename replacement");

    old_fd = open(PERSIST_DIR "/unlinked", O_CREAT | O_EXCL | O_RDWR, 0644);
    if (old_fd < 0 || write_exact(old_fd, "open", 4u) ||
        unlink(PERSIST_DIR "/unlinked") || lseek(old_fd, 0, SEEK_SET) != 0 ||
        read_exact(old_fd, verify, 4u) || memcmp(verify, "open", 4u) || close(old_fd))
        return fail("unlink-open-file");

    directory = opendir(PERSIST_DIR);
    if (!directory || stat(PERSIST_DIR "/data.txt", &status) ||
        status.st_size != DATA_SIZE) return fail("stat/readdir setup");
    while ((entry = readdir(directory)) != 0)
        if (!strcmp(entry->d_name, "data.txt")) found_data = 1;
    if (!found_data || closedir(directory)) return fail("readdir");

    if (write_file(PERSIST_DIR "/hello.vix", hello_source,
                   sizeof(hello_source) - 1u, 0644) ||
        run("/bin/vixc", compile, 1) || execute_generated("first"))
        return fail("native vix compile/exec");

    if (write_file(PERSIST_DIR "/invalid.vix", "fn broken( {\n", 13u, 0644) ||
        write_file(PERSIST_DIR "/preserved", "preserved", 9u, 0644) ||
        run("/bin/vixc", bad_compile, 0) ||
        !file_equals(PERSIST_DIR "/preserved", "preserved", 9u) ||
        unlink(PERSIST_DIR "/invalid.vix")) return fail("existing output preservation");

    if (write_file(PERSIST_DIR "/first.complete", "complete\n", 9u, 0644))
        return fail("completion marker fsync");
    puts("[persistent_work_first_boot] PASS");
    return 0;
}

static int second_boot(void)
{
    unsigned char expected[DATA_SIZE];
    char buffer[32];
    int fd;
    fill_data(expected);
    if (!file_equals(PERSIST_DIR "/data.txt", expected, sizeof(expected)) ||
        !file_equals(PERSIST_DIR "/hello.vix", hello_source,
                     sizeof(hello_source) - 1u) ||
        path_exists(PERSIST_DIR "/old-name") ||
        path_exists(PERSIST_DIR "/unlinked") ||
        path_exists(PERSIST_DIR "/invalid.vix"))
        return fail("second-boot namespace/data validation");
    if (execute_generated("second")) return fail("second-boot generated exec");
    fd = open(PERSIST_DIR "/data.txt", O_RDWR | O_APPEND);
    if (fd < 0 || write_exact(fd, "second", 6u) || fsync(fd) ||
        ftruncate(fd, DATA_SIZE + 3u) || fsync(fd) || close(fd) ||
        (fd = open(PERSIST_DIR "/data.txt", O_RDONLY)) < 0 ||
        lseek(fd, DATA_SIZE, SEEK_SET) != DATA_SIZE || read_exact(fd, buffer, 3u) ||
        memcmp(buffer, "sec", 3u) || close(fd)) return fail("second-boot mutation");
    puts("[persistent_work_second_boot] PASS");
    return 0;
}

int main(void)
{
    int marker;
    struct stat directory_status;
    if (mkdir(PERSIST_DIR, 0755) != 0 &&
        (stat(PERSIST_DIR, &directory_status) != 0 ||
         (directory_status.st_mode & S_IFMT) != S_IFDIR))
        return fail("mkdir persist");
    marker = open(PERSIST_DIR "/first.complete", O_RDONLY);
    if (marker < 0) return first_boot();
    if (close(marker)) return fail("close completion marker");
    return second_boot();
}
