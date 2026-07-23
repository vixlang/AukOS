#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_DIR "/work/vixc-test"
#define MAX_CAPTURE 4096u
#define MAX_ELF_SIZE (256u * 1024u)

struct elf64_header {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};

struct elf64_program {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
};

struct elf64_section {
    uint32_t name, type;
    uint64_t flags, address, offset, size;
    uint32_t link, info;
    uint64_t alignment, entry_size;
};

struct elf64_symbol {
    uint32_t name;
    unsigned char info, other;
    uint16_t shndx;
    uint64_t value, size;
};

struct elf64_rela { uint64_t offset, info; int64_t addend; };

static const char generated_source[] =
    "extern \"C\"\n"
    "{\n"
    "    fn aukos_vix_write_str(text: string): i64\n"
    "    fn aukos_vix_streq(left: string, right: string): i32\n"
    "    fn aukos_vix_open(path: string, flags: i32, mode: i32): i32\n"
    "    fn aukos_vix_write_fd(fd: i32, data: string, size: usize): i64\n"
    "    fn aukos_vix_fsync(fd: i32): i32\n"
    "    fn aukos_vix_close(fd: i32): i32\n"
    "}\n"
    "fn local_sum(limit: i32): i32\n"
    "{\n"
    "    let mut total = 0\n"
    "    for (i in 0 .. limit) { total = total + i }\n"
    "    return total\n"
    "}\n"
    "fn main(argc: i32, argv: &string, envp: &string): i32\n"
    "{\n"
    "    if (argc != 2 or aukos_vix_streq(argv[1], \"guest-arg\") == 0) { return 2 }\n"
    "    let mut found_env = 0\n"
    "    let mut index = 0\n"
    "    while (index < 16 and envp[index] != nil)\n"
    "    {\n"
    "        if (aukos_vix_streq(envp[index], \"VIX_GENERATED=1\") == 1) { found_env = 1 }\n"
    "        index += 1\n"
    "    }\n"
    "    if (found_env == 0 or local_sum(6) != 15) { return 3 }\n"
    "    let fd = aukos_vix_open(\"generated.data\", 577, 420)\n"
    "    if (fd < 0) { return 4 }\n"
    "    if (aukos_vix_write_fd(fd, \"guest-data\", 10) != 10) { return 5 }\n"
    "    if (aukos_vix_fsync(fd) != 0 or aukos_vix_close(fd) != 0) { return 6 }\n"
    "    if (aukos_vix_write_str(\"[vixc_generated_exec_test] PASS\\n\") < 0) { return 7 }\n"
    "    return 0\n"
    "}\n";

static const char direct_source[] =
    "fn main(): i32\n"
    "{\n"
    "    print(\"[vixc_direct_driver_test] PASS\")\n"
    "    return 0\n"
    "}\n";

static const char unresolved_source[] =
    "extern \"C\" { fn vixc_intentionally_missing(): i32 }\n"
    "fn main(): i32 { return vixc_intentionally_missing() }\n";

static int last_tool_status;

static int fail(const char *reason)
{
    printf("[vixc_test] FAIL %s\n", reason);
    return 1;
}

static int fail_capture(const char *reason, const char *capture)
{
    printf("[vixc_test] FAIL %s status=%d: %s\n", reason, last_tool_status,
           capture && *capture ? capture : "no diagnostic");
    return 1;
}

static int write_file(const char *path, const char *data)
{
    size_t size = strlen(data), offset = 0;
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return -1;
    while (offset < size) {
        ssize_t count = write(fd, data + offset, size - offset);
        if (count <= 0) { close(fd); return -1; }
        offset += (size_t)count;
    }
    if (fsync(fd) != 0 || close(fd) != 0) return -1;
    return 0;
}

static int file_equals(const char *path, const char *expected)
{
    char buffer[64];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    ssize_t count = read(fd, buffer, sizeof(buffer));
    int close_result = close(fd);
    size_t expected_size = strlen(expected);
    return close_result == 0 && count == (ssize_t)expected_size &&
           !memcmp(buffer, expected, expected_size);
}

static int count_driver_temporaries(void)
{
    DIR *directory = opendir("/tmp");
    if (!directory) return -1;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != 0)
        if (!strncmp(entry->d_name, "vixc-object-", 12)) count++;
    if (closedir(directory) != 0) return -1;
    return count;
}

static int run_captured(char *const argv[], int expect_success,
                        char *capture, size_t capacity)
{
    extern char **environ;
    int descriptors[2], status = -1;
    size_t used = 0;
    if (pipe(descriptors) != 0) return -1;
    pid_t child = fork();
    if (child == 0) {
        close(descriptors[0]);
        if (dup2(descriptors[1], 1) < 0 || dup2(descriptors[1], 2) < 0)
            _exit(126);
        close(descriptors[1]);
        execve("/bin/vixc", argv, environ);
        _exit(127);
    }
    close(descriptors[1]);
    if (child < 0) { close(descriptors[0]); return -1; }
    for (;;) {
        char discard[256];
        char *destination = used + 1 < capacity ? capture + used : discard;
        size_t available = used + 1 < capacity ? capacity - used - 1 : sizeof(discard);
        ssize_t count = read(descriptors[0], destination, available);
        if (count < 0) { close(descriptors[0]); return -1; }
        if (count == 0) break;
        if (destination != discard) used += (size_t)count;
    }
    close(descriptors[0]);
    if (waitpid(child, &status, 0) < 0) return -1;
    last_tool_status = status;
    if (capacity) capture[used] = '\0';
    return expect_success ? (status == 0 ? 0 : -1) : (status != 0 && used ? 0 : -1);
}

static int range_valid(uint64_t offset, uint64_t size, size_t total)
{
    return offset <= total && size <= total - offset;
}

static unsigned char *read_elf(const char *path, size_t *size_out)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    off_t end = lseek(fd, 0, SEEK_END);
    if (end <= 0 || (uint64_t)end > MAX_ELF_SIZE || lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return 0;
    }
    unsigned char *bytes = malloc((size_t)end);
    if (!bytes) { close(fd); return 0; }
    size_t offset = 0;
    while (offset < (size_t)end) {
        ssize_t count = read(fd, bytes + offset, (size_t)end - offset);
        if (count <= 0) { free(bytes); close(fd); return 0; }
        offset += (size_t)count;
    }
    close(fd);
    *size_out = (size_t)end;
    return bytes;
}

static int string_valid(const char *table, size_t size, uint32_t offset)
{
    return offset < size && memchr(table + offset, '\0', size - offset) != 0;
}

static int validate_object(const char *path)
{
    size_t size;
    unsigned char *bytes = read_elf(path, &size);
    if (!bytes || size < sizeof(struct elf64_header)) { free(bytes); return -1; }
    struct elf64_header *header = (struct elf64_header *)bytes;
    if (memcmp(header->ident, "\177ELF\2\1\1", 7) || header->type != 1 ||
        header->machine != 62 || header->shentsize != sizeof(struct elf64_section) ||
        header->shnum == 0 || header->shnum > 32 ||
        !range_valid(header->shoff,
                     (uint64_t)header->shnum * sizeof(struct elf64_section), size) ||
        header->shstrndx >= header->shnum) { free(bytes); return -1; }
    struct elf64_section *sections =
        (struct elf64_section *)(bytes + header->shoff);
    struct elf64_section *names = &sections[header->shstrndx];
    if (!range_valid(names->offset, names->size, size)) { free(bytes); return -1; }
    const char *shstr = (const char *)(bytes + names->offset);
    int found_text = 0, found_rodata = 0, found_symtab = 0, found_rela = 0;
    int found_main = 0, relocation_count = 0;
    for (int index = 0; index < header->shnum; index++) {
        struct elf64_section *section = &sections[index];
        if (!string_valid(shstr, names->size, section->name) ||
            (section->type != 8 && !range_valid(section->offset, section->size, size))) {
            free(bytes); return -1;
        }
        const char *name = shstr + section->name;
        found_text |= strcmp(name, ".text") == 0;
        found_rodata |= strcmp(name, ".rodata") == 0 && section->size != 0;
        if (section->type == 2) {
            if (section->entry_size != sizeof(struct elf64_symbol) ||
                section->size % sizeof(struct elf64_symbol) || section->link >= header->shnum) {
                free(bytes); return -1;
            }
            struct elf64_section *strings = &sections[section->link];
            if (!range_valid(strings->offset, strings->size, size)) { free(bytes); return -1; }
            const char *strtab = (const char *)(bytes + strings->offset);
            struct elf64_symbol *symbols =
                (struct elf64_symbol *)(bytes + section->offset);
            size_t count = section->size / sizeof(*symbols);
            for (size_t symbol = 1; symbol < count; symbol++) {
                if (!string_valid(strtab, strings->size, symbols[symbol].name)) {
                    free(bytes); return -1;
                }
                if (!strcmp(strtab + symbols[symbol].name, "main") &&
                    symbols[symbol].shndx != 0) found_main = 1;
            }
            found_symtab = 1;
        }
        if (section->type == 4) {
            if (section->entry_size != sizeof(struct elf64_rela) ||
                section->size % sizeof(struct elf64_rela)) { free(bytes); return -1; }
            struct elf64_rela *relocations =
                (struct elf64_rela *)(bytes + section->offset);
            size_t count = section->size / sizeof(*relocations);
            for (size_t relocation = 0; relocation < count; relocation++) {
                uint32_t type = (uint32_t)relocations[relocation].info;
                if (type != 2 && type != 4) { free(bytes); return -1; }
                relocation_count++;
            }
            found_rela = 1;
        }
    }
    free(bytes);
    return found_text && found_rodata && found_symtab && found_rela &&
                   found_main && relocation_count > 0 ? 0 : -1;
}

static int validate_executable(const char *path)
{
    size_t size;
    unsigned char *bytes = read_elf(path, &size);
    if (!bytes || size < sizeof(struct elf64_header)) { free(bytes); return -1; }
    struct elf64_header *header = (struct elf64_header *)bytes;
    if (memcmp(header->ident, "\177ELF\2\1\1", 7) || header->type != 2 ||
        header->machine != 62 || header->phentsize != sizeof(struct elf64_program) ||
        header->phnum != 3 ||
        !range_valid(header->phoff,
                     (uint64_t)header->phnum * sizeof(struct elf64_program), size)) {
        free(bytes); return -1;
    }
    struct elf64_program *programs =
        (struct elf64_program *)(bytes + header->phoff);
    int rx = 0, read_only = 0, rw = 0, entry_ok = 0;
    for (int index = 0; index < header->phnum; index++) {
        struct elf64_program *program = &programs[index];
        if (program->type != 1 || (program->flags & 3) == 3 ||
            !range_valid(program->offset, program->filesz, size) ||
            program->memsz < program->filesz) { free(bytes); return -1; }
        rx |= program->flags == 5;
        read_only |= program->flags == 4;
        rw |= program->flags == 6;
        if (program->flags == 5 && header->entry >= program->vaddr &&
            header->entry < program->vaddr + program->memsz) entry_ok = 1;
    }
    free(bytes);
    return rx && read_only && rw && entry_ok ? 0 : -1;
}

static int run_generated(void)
{
    char *argv[] = {"generated.elf", "guest-arg", 0};
    char *envp[] = {"PATH=/bin", "VIX_GENERATED=1", 0};
    int status = -1;
    pid_t child = fork();
    if (child == 0) {
        execve(TEST_DIR "/generated.elf", argv, envp);
        _exit(127);
    }
    if (child < 0 || waitpid(child, &status, 0) < 0) return -1;
    return status == 0 ? 0 : -1;
}

static int run_direct_generated(void)
{
    char *argv[] = {"generated-direct", 0};
    char *envp[] = {"PATH=/bin", 0};
    int status = -1;
    pid_t child = fork();
    if (child == 0) {
        execve(TEST_DIR "/generated-direct", argv, envp);
        _exit(127);
    }
    if (child < 0 || waitpid(child, &status, 0) < 0) return -1;
    return status == 0 ? 0 : -1;
}

int main(void)
{
    char capture[MAX_CAPTURE];
    struct stat directory_status;
    if (mkdir(TEST_DIR, 0755) != 0 &&
        (stat(TEST_DIR, &directory_status) != 0 ||
         (directory_status.st_mode & S_IFMT) != S_IFDIR)) return fail("mkdir");
    if (write_file(TEST_DIR "/success.vix", generated_source) != 0 ||
        write_file(TEST_DIR "/direct.vix", direct_source) != 0 ||
        write_file(TEST_DIR "/unresolved.vix", unresolved_source) != 0 ||
        write_file(TEST_DIR "/frontend.vix", "fn main(): i32 { return 0 }\n") != 0 ||
        write_file(TEST_DIR "/lexer.vix", "fn main(): i32 { let bad = \"unterminated\n") != 0 ||
        write_file(TEST_DIR "/parser.vix", "fn broken( {\n") != 0 ||
        write_file(TEST_DIR "/type.vix", "fn bad(): i32 { return \"wrong\" }\n") != 0)
        return fail("write sources");

    char *help[] = {"vixc", "--help", 0};
    char *version[] = {"vixc", "--version", 0};
    char *unknown[] = {"vixc", "--unknown", 0};
    char *missing_output[] = {"vixc", "-o", 0};
    char *missing_target[] = {"vixc", "--target", 0};
    char *multiple_input[] = {"vixc", TEST_DIR "/frontend.vix",
                              TEST_DIR "/success.vix", 0};
    char *conflicting_mode[] = {"vixc", "-obj", "--link",
                                TEST_DIR "/frontend.vix", 0};
    if (run_captured(help, 1, capture, sizeof(capture)) != 0 ||
        !strstr(capture, "vixc INPUT.vix [-o OUTPUT]") ||
        !strstr(capture, "--target TRIPLE") || strstr(capture, "LLVM") ||
        strstr(capture, "--backend"))
        return fail_capture("help", capture);
    if (run_captured(version, 1, capture, sizeof(capture)) != 0 ||
        strcmp(capture, "vixc 0.4.6\n"))
        return fail_capture("version", capture);
    if (run_captured(unknown, 0, capture, sizeof(capture)) != 0)
        return fail("unknown option");
    if (run_captured(missing_output, 0, capture, sizeof(capture)) != 0)
        return fail("missing output argument");
    if (run_captured(missing_target, 0, capture, sizeof(capture)) != 0)
        return fail("missing target argument");
    if (run_captured(multiple_input, 0, capture, sizeof(capture)) != 0)
        return fail("multiple input");
    if (run_captured(conflicting_mode, 0, capture, sizeof(capture)) != 0)
        return fail("conflicting mode");

    char *lex_ok[] = {"vixc", "--lex", TEST_DIR "/frontend.vix", 0};
    char *parse_ok[] = {"vixc", "--parser", TEST_DIR "/frontend.vix", 0};
    char *check_ok[] = {"vixc", "--check", TEST_DIR "/frontend.vix", 0};
    char *mir_ok[] = {"vixc", "--debug=mir", TEST_DIR "/frontend.vix", 0};
    char *asm_ok[] = {"vixc", "-S", TEST_DIR "/frontend.vix", "-o",
                      TEST_DIR "/success.s", 0};
    char *lex_bad[] = {"vixc", "--lex", TEST_DIR "/lexer.vix", 0};
    char *parse_bad[] = {"vixc", "--parser", TEST_DIR "/parser.vix", 0};
    char *type_bad[] = {"vixc", "--check", TEST_DIR "/type.vix", 0};
    if (run_captured(lex_ok, 1, capture, sizeof(capture)) != 0)
        return fail_capture("lex success", capture);
    if (run_captured(parse_ok, 1, capture, sizeof(capture)) != 0) return fail("parser success");
    if (run_captured(check_ok, 1, capture, sizeof(capture)) != 0) return fail("check success");
    if (run_captured(mir_ok, 1, capture, sizeof(capture)) != 0) return fail("mir success");
    if (run_captured(asm_ok, 1, capture, sizeof(capture)) != 0) return fail("assembly success");
    if (run_captured(lex_bad, 0, capture, sizeof(capture)) != 0) return fail("lexer failure");
    if (run_captured(parse_bad, 0, capture, sizeof(capture)) != 0) return fail("parser failure");
    if (run_captured(type_bad, 0, capture, sizeof(capture)) != 0) return fail("type failure");

    if (chdir(TEST_DIR) != 0) return fail("chdir");

    if (write_file("existing-output", "preserve-me") != 0)
        return fail("write existing output");
    int temporary_baseline = count_driver_temporaries();
    if (temporary_baseline < 0) return fail("count temporary baseline");
    char *compile_failure[] = {"vixc", TEST_DIR "/parser.vix", "-o",
                               TEST_DIR "/compile-failure-output", 0};
    char *link_failure[] = {"vixc", TEST_DIR "/unresolved.vix", "-o",
                            TEST_DIR "/existing-output", 0};
    if (run_captured(compile_failure, 0, capture, sizeof(capture)) != 0)
        return fail("default compile failure");
    if (run_captured(link_failure, 0, capture, sizeof(capture)) != 0 ||
        !strstr(capture, "link stage"))
        return fail_capture("default link failure", capture);
    if (!file_equals("existing-output", "preserve-me"))
        return fail("existing output preservation");
    if (count_driver_temporaries() != temporary_baseline)
        return fail("failure temporary cleanup");
    if (unlink("existing-output") != 0) return fail("existing output unlink");

    char *dash_compile[] = {"vixc", "-obj", "-o", TEST_DIR "/dash.o",
                            "--", "-source.vix", 0};
    if (write_file("-source.vix", generated_source) != 0 ||
        run_captured(dash_compile, 1, capture, sizeof(capture)) != 0 ||
        validate_object("dash.o") != 0 || unlink("dash.o") != 0 ||
        unlink("-source.vix") != 0)
        return fail("option terminator");

    char *default_object[] = {"vixc", "-obj", TEST_DIR "/direct.vix", "-o",
                              TEST_DIR "/default-link.o", 0};
    char *default_link[] = {"vixc", "--link", TEST_DIR "/default-link.o", 0};
    if (run_captured(default_object, 1, capture, sizeof(capture)) != 0 ||
        run_captured(default_link, 1, capture, sizeof(capture)) != 0 ||
        validate_executable(TEST_DIR "/default-link") != 0 ||
        unlink("default-link.o") != 0 || unlink("default-link") != 0)
        return fail("default link output");

    for (int iteration = 0; iteration < 16; iteration++) {
        char *compile[] = {"vixc", "-obj", TEST_DIR "/success.vix", "-o",
                           TEST_DIR "/generated.o", 0};
        char *link[] = {"vixc", "--link", TEST_DIR "/generated.o", "-o",
                        TEST_DIR "/generated.elf", 0};
        if (run_captured(compile, 1, capture, sizeof(capture)) != 0)
            return fail("compile object");
        if (validate_object(TEST_DIR "/generated.o") != 0)
            return fail("object audit");
        if (run_captured(link, 1, capture, sizeof(capture)) != 0)
            return fail("static link");
        if (validate_executable(TEST_DIR "/generated.elf") != 0)
            return fail("executable audit");
        if (run_generated() != 0) return fail("generated exec");
        int fd = open("generated.data", O_RDONLY);
        char data[10];
        if (fd < 0 || read(fd, data, sizeof(data)) != (ssize_t)sizeof(data) ||
            memcmp(data, "guest-data", sizeof(data)) != 0 || close(fd) != 0)
            return fail("generated cwd/file");
        if (unlink("generated.data") != 0 || unlink("generated.o") != 0 ||
            unlink("generated.elf") != 0)
            return fail("reuse unlink");
    }
    for (int iteration = 0; iteration < 16; iteration++) {
        char *direct[] = {"vixc", TEST_DIR "/direct.vix", "-o",
                          TEST_DIR "/generated-direct", 0};
        if (run_captured(direct, 1, capture, sizeof(capture)) != 0)
            return fail_capture("direct compile", capture);
        if (validate_executable(TEST_DIR "/generated-direct") != 0)
            return fail("direct executable audit");
        if (count_driver_temporaries() != temporary_baseline)
            return fail("success temporary cleanup");
        if (run_direct_generated() != 0) return fail("direct generated exec");
        if (unlink("generated-direct") != 0) return fail("direct reuse unlink");
    }
    puts("[vixc_frontend_test] PASS");
    puts("[vixc_object_test] PASS");
    puts("[vixc_cli_test] PASS");
    puts("[vixc_temp_cleanup_test] PASS");
    puts("[vixc_output_preservation_test] PASS");
    return 0;
}
