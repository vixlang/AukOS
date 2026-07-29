#include <stdint.h>

#include <aukos/shell_parse.h>

#define SYS_EXIT 60u
#define SYS_WRITE 1u
#define SYS_READ 0u
#define SYS_FSINFO 1027u
#define SYS_FORK 57u
#define SYS_EXECVE 59u
#define SYS_WAITPID 61u
#define SYS_OPEN 2u
#define SYS_CLOSE 3u
#define SYS_PIPE 22u
#define SYS_DUP2 33u
#define SYS_CHDIR 80u
#define SYS_GETCWD 1028u
#define O_RDONLY 0u
#define O_WRONLY 1u
#define O_CREAT 0100u
#define O_TRUNC 01000u
#define O_APPEND 02000u
#define UINT64_MAX_VALUE 0xffffffffffffffffull
#define SHELL_ENV_MAX 16u
#define SHELL_PATH_MAX 256u

struct fsinfo {
    uint32_t tmpfs_ready;
    uint32_t ext4_ready;
    uint32_t fat32_ready;
    uint32_t block_devices;
};

static char *default_environment[] = {
    "PATH=/bin", "HOME=/", "TMPDIR=/tmp", 0
};
static char *shell_environment[SHELL_ENV_MAX];
static char pwd_entry[sizeof("PWD=") + SHELL_PATH_MAX];
static char oldpwd_entry[sizeof("OLDPWD=") + SHELL_PATH_MAX];

static uint64_t syscall0(uint64_t number)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number) : "rcx", "r11", "memory");
    return result;
}

static uint64_t syscall1(uint64_t number, uint64_t arg0)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0) : "rcx", "r11", "memory");
    return result;
}

static uint64_t syscall2(uint64_t number, uint64_t arg0, uint64_t arg1)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0), "S"(arg1) : "rcx", "r11", "memory");
    return result;
}

static uint64_t syscall3(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    uint64_t result;
    __asm__ volatile ("syscall" : "=a"(result) : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2) : "rcx", "r11", "memory");
    return result;
}

static uint64_t strlen(const char *str)
{
    uint64_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static void write_str(const char *str)
{
    syscall3(SYS_WRITE, 1, (uint64_t)str, strlen(str));
}

static void write_prompt(void)
{
    char cwd[SHELL_PATH_MAX];

    write_str("[root@aukos ");
    if (syscall2(SYS_GETCWD, (uint64_t)cwd, sizeof(cwd)) != UINT64_MAX_VALUE) {
        write_str(cwd);
    } else {
        write_str("?");
    }
    write_str("]$ ");
}

static int string_equals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == *right;
}

static int environment_name_matches(const char *entry, const char *name)
{
    while (*name && *entry == *name) {
        entry++;
        name++;
    }
    return *name == '\0' && *entry == '=';
}

static const char *environment_value(const char *name)
{
    for (uint64_t index = 0; index < SHELL_ENV_MAX && shell_environment[index];
         index++) {
        if (environment_name_matches(shell_environment[index], name)) {
            const char *entry = shell_environment[index];

            while (*entry != '=') {
                entry++;
            }
            return entry + 1;
        }
    }
    return 0;
}

static int set_path_entry(char *entry, uint64_t capacity, const char *name,
                          const char *path)
{
    uint64_t offset = 0;

    while (*name) {
        if (offset + 1u >= capacity) {
            return -1;
        }
        entry[offset++] = *name++;
    }
    if (offset + 1u >= capacity) {
        return -1;
    }
    entry[offset++] = '=';
    while (*path) {
        if (offset + 1u >= capacity) {
            return -1;
        }
        entry[offset++] = *path++;
    }
    entry[offset] = '\0';
    return 0;
}

static void initialize_environment(char **envp)
{
    char cwd[SHELL_PATH_MAX];
    uint64_t count = 0;
    char **source = envp && envp[0] ? envp : default_environment;

    for (uint64_t index = 0; source[index] && count + 3u < SHELL_ENV_MAX;
         index++) {
        if (!environment_name_matches(source[index], "PWD") &&
            !environment_name_matches(source[index], "OLDPWD")) {
            shell_environment[count++] = source[index];
        }
    }
    if (syscall2(SYS_GETCWD, (uint64_t)cwd, sizeof(cwd)) == UINT64_MAX_VALUE) {
        cwd[0] = '/';
        cwd[1] = '\0';
    }
    set_path_entry(pwd_entry, sizeof(pwd_entry), "PWD", cwd);
    set_path_entry(oldpwd_entry, sizeof(oldpwd_entry), "OLDPWD", cwd);
    shell_environment[count++] = pwd_entry;
    shell_environment[count++] = oldpwd_entry;
    shell_environment[count] = 0;
}

static int string_has_slash(const char *value)
{
    while (*value) {
        if (*value++ == '/') {
            return 1;
        }
    }
    return 0;
}

static void strip_newline(char *str)
{
    for (uint64_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n' || str[i] == '\r') {
            str[i] = '\0';
            return;
        }
    }
}

static void applet_help(void)
{
    write_str("Welcome to the aush!\n");
    write_str("type help to get tips\n");
}

static int applet_cd(const struct shell_command *command)
{
    const char *destination = 0;
    const char *pwd = environment_value("PWD");
    char cwd[SHELL_PATH_MAX];
    char previous[SHELL_PATH_MAX];
    uint64_t argument = 1u;

    if (pwd) {
        uint64_t index = 0;

        while (pwd[index] && index + 1u < sizeof(previous)) {
            previous[index] = pwd[index];
            index++;
        }
        if (pwd[index] != '\0') {
            write_str("cd: PWD is too long\n");
            return -1;
        }
        previous[index] = '\0';
    } else {
        previous[0] = '/';
        previous[1] = '\0';
    }

    while (argument < command->argc &&
           (string_equals(command->argv[argument], "-L") ||
            string_equals(command->argv[argument], "-P"))) {
        argument++;
    }
    if (argument < command->argc && string_equals(command->argv[argument], "--")) {
        argument++;
    }
    if (argument + 1u < command->argc) {
        write_str("cd: too many arguments\n");
        return -1;
    }
    if (argument < command->argc) {
        destination = command->argv[argument];
    } else {
        destination = environment_value("HOME");
        if (!destination || !destination[0]) {
            destination = "/";
        }
    }
    if (string_equals(destination, "-")) {
        destination = environment_value("OLDPWD");
        if (!destination || !destination[0]) {
            write_str("cd: OLDPWD not set\n");
            return -1;
        }
    }
    if (syscall1(SYS_CHDIR, (uint64_t)destination) == UINT64_MAX_VALUE) {
        write_str("cd: ");
        write_str(destination);
        write_str(": failed\n");
        return -1;
    }
    if (syscall2(SYS_GETCWD, (uint64_t)cwd, sizeof(cwd)) == UINT64_MAX_VALUE ||
        set_path_entry(oldpwd_entry, sizeof(oldpwd_entry), "OLDPWD", previous) != 0 ||
        set_path_entry(pwd_entry, sizeof(pwd_entry), "PWD", cwd) != 0) {
        write_str("cd: failed to update PWD\n");
        return -1;
    }
    return 0;
}

static int run_program(char *path, char *name)
{
    char *argv[2];
    uint64_t pid;
    uint64_t waited;
    int status = 1;

    argv[0] = name;
    argv[1] = 0;
    pid = syscall0(SYS_FORK);
    if (pid == 0) {
        syscall3(SYS_EXECVE, (uint64_t)path, (uint64_t)argv, 0);
        write_str("execve failed\n");
        syscall1(SYS_EXIT, 1);
    } else if (pid != UINT64_MAX_VALUE) {
        waited = syscall3(SYS_WAITPID, pid, (uint64_t)&status, 0);
        if (waited == UINT64_MAX_VALUE) {
            write_str("waitpid failed\n");
            return -1;
        }
        return status;
    } else {
        write_str("fork failed\n");
    }
    return -1;
}

static void applet_fs(void)
{
    struct fsinfo info;

    if (syscall2(SYS_FSINFO, (uint64_t)&info, sizeof(info)) == UINT64_MAX_VALUE) {
        write_str("fs: status unavailable\n");
        return;
    }

    write_str("fs: tmpfs=");
    write_str(info.tmpfs_ready ? "ok" : "missing");
    write_str(" ext4=");
    write_str(info.ext4_ready ? "ok" : "missing");
    write_str(" fat32=");
    write_str(info.fat32_ready ? "ok" : "missing");
    write_str(" block=ramdisk+virtio0\n");
}

static void close_pipeline_fds(int pipes[SHELL_PARSE_MAX_STAGES - 1u][2],
                               size_t pipe_count)
{
    for (size_t index = 0; index < pipe_count; index++) {
        if (pipes[index][0] >= 0) {
            syscall1(SYS_CLOSE, (uint64_t)pipes[index][0]);
            pipes[index][0] = -1;
        }
        if (pipes[index][1] >= 0) {
            syscall1(SYS_CLOSE, (uint64_t)pipes[index][1]);
            pipes[index][1] = -1;
        }
    }
}

static void exec_external(struct shell_command *command)
{
    char path[128];
    uint64_t length;

    if (string_has_slash(command->argv[0])) {
        syscall3(SYS_EXECVE, (uint64_t)command->argv[0],
                 (uint64_t)command->argv, (uint64_t)shell_environment);
    } else {
        const char prefix[] = "/bin/";

        length = strlen(command->argv[0]);
        if (length + sizeof(prefix) <= sizeof(path)) {
            for (uint64_t index = 0; index + 1u < sizeof(prefix); index++) {
                path[index] = prefix[index];
            }
            for (uint64_t index = 0; index <= length; index++) {
                path[sizeof(prefix) - 1u + index] = command->argv[0][index];
            }
            syscall3(SYS_EXECVE, (uint64_t)path, (uint64_t)command->argv,
                     (uint64_t)shell_environment);
        }
        syscall3(SYS_EXECVE, (uint64_t)"/bin/toybox",
                 (uint64_t)command->argv, (uint64_t)shell_environment);
    }
    write_str("execve failed\n");
    syscall1(SYS_EXIT, 127u);
    for (;;) {}
}

static int run_pipeline(struct shell_pipeline *pipeline)
{
    int pipes[SHELL_PARSE_MAX_STAGES - 1u][2];
    uint64_t pids[SHELL_PARSE_MAX_STAGES];
    size_t pipe_count = pipeline->stage_count - 1u;
    size_t child_count = 0;
    int result = 0;

    for (size_t index = 0; index < pipe_count; index++) {
        pipes[index][0] = -1;
        pipes[index][1] = -1;
    }
    for (size_t index = 0; index < pipe_count; index++) {
        if (syscall1(SYS_PIPE, (uint64_t)pipes[index]) == UINT64_MAX_VALUE) {
            close_pipeline_fds(pipes, pipe_count);
            write_str("aush: pipe failed\n");
            return -1;
        }
    }

    for (size_t stage = 0; stage < pipeline->stage_count; stage++) {
        uint64_t pid = syscall0(SYS_FORK);

        if (pid == 0u) {
            struct shell_command *command = &pipeline->stages[stage];
            uint64_t fd;

            if (stage != 0u &&
                syscall2(SYS_DUP2, (uint64_t)pipes[stage - 1u][0], 0u) ==
                    UINT64_MAX_VALUE) {
                syscall1(SYS_EXIT, 126u);
            }
            if (stage + 1u < pipeline->stage_count &&
                syscall2(SYS_DUP2, (uint64_t)pipes[stage][1], 1u) ==
                    UINT64_MAX_VALUE) {
                syscall1(SYS_EXIT, 126u);
            }
            if (command->input_path) {
                fd = syscall3(SYS_OPEN, (uint64_t)command->input_path,
                              O_RDONLY, 0u);
                if (fd == UINT64_MAX_VALUE ||
                    syscall2(SYS_DUP2, fd, 0u) == UINT64_MAX_VALUE) {
                    syscall1(SYS_EXIT, 126u);
                }
                syscall1(SYS_CLOSE, fd);
            }
            if (command->output_path) {
                uint64_t flags = O_CREAT | O_WRONLY |
                    (command->output_append ? O_APPEND : O_TRUNC);

                fd = syscall3(SYS_OPEN, (uint64_t)command->output_path,
                              flags, 0644u);
                if (fd == UINT64_MAX_VALUE ||
                    syscall2(SYS_DUP2, fd, 1u) == UINT64_MAX_VALUE) {
                    syscall1(SYS_EXIT, 126u);
                }
                syscall1(SYS_CLOSE, fd);
            }
            close_pipeline_fds(pipes, pipe_count);
            exec_external(command);
        }
        if (pid == UINT64_MAX_VALUE) {
            result = -1;
            write_str("aush: fork failed\n");
            break;
        }
        pids[child_count++] = pid;
    }

    close_pipeline_fds(pipes, pipe_count);
    for (size_t index = 0; index < child_count; index++) {
        int status = 1;

        if (syscall3(SYS_WAITPID, pids[index], (uint64_t)&status, 0u) ==
            UINT64_MAX_VALUE) {
            write_str("aush: waitpid failed\n");
            result = -1;
        } else if (index + 1u == child_count && status != 0) {
            result = status;
        }
    }
    return result;
}

static int run_command(char *line, uint64_t line_capacity)
{
    struct shell_pipeline pipeline;
    struct shell_command *command;
    enum shell_parse_result parse_result;

    parse_result = shell_parse_pipeline(line, line_capacity, &pipeline);
    if (parse_result != SHELL_PARSE_OK) {
        write_str("aush: parse error: ");
        write_str(shell_parse_result_string(parse_result));
        write_str("\n");
        return -1;
    }
    if (pipeline.stage_count == 0u) {
        return 0;
    }
    command = &pipeline.stages[0];

    if (pipeline.stage_count == 1u && !command->input_path &&
        !command->output_path && string_equals(command->argv[0], "help")) {
        applet_help();
    } else if (pipeline.stage_count == 1u && !command->input_path &&
               !command->output_path && string_equals(command->argv[0], "fs")) {
        applet_fs();
    } else if (pipeline.stage_count == 1u && !command->input_path &&
               !command->output_path && string_equals(command->argv[0], "cd")) {
        return applet_cd(command);
    } else if (pipeline.stage_count == 1u && !command->input_path &&
               !command->output_path && string_equals(command->argv[0], "signal_test")) {
        return run_program("/bin/signal_test", "signal_test");
    } else if (pipeline.stage_count == 1u && !command->input_path &&
               !command->output_path && string_equals(command->argv[0], "exit")) {
        syscall1(SYS_EXIT, 0);
    } else {
        return run_pipeline(&pipeline);
    }
    return 0;
}

static int run_startup_command(const char *command)
{
    char line[512];
    uint64_t length = 0;

    while (command[length] != '\0' && length + 1 < sizeof(line)) {
        line[length] = command[length];
        length++;
    }
    if (command[length] != '\0') {
        write_str("aush: startup command too long\n");
        return -1;
    }
    line[length] = '\0';
    write_prompt();
    write_str(line);
    write_str("\n");
    return run_command(line, sizeof(line));
}

static int smoke_tests_requested(void) {
  static const char marker[] = "/work/.aukos-smoke";
  uint64_t fd = syscall3(SYS_OPEN, (uint64_t)marker, O_RDONLY, 0u);

  if (fd == UINT64_MAX_VALUE) {
    return 0;
  }
  (void)syscall1(SYS_CLOSE, fd);
  return 1;
}

void _start(uint64_t argc, char **argv, char **envp) {
  char line[128];
  int applets_ok = 1;
  int true_status;
  int false_status;

  (void)argc;
  (void)argv;
  initialize_environment(envp);
  applet_help();

  if (smoke_tests_requested()) {
    applets_ok &= run_startup_command("signal_test") == 0;
    write_prompt();
    write_str("/bin/dev_memory_test\n");
    applets_ok &= run_program("/bin/dev_memory_test", "dev_memory_test") == 0;
    write_prompt();
    write_str("/bin/pipe_test\n");
    applets_ok &= run_program("/bin/pipe_test", "pipe_test") == 0;
    write_prompt();
    write_str("/bin/process_env_test\n");
    applets_ok &= run_program("/bin/process_env_test", "process_env_test") == 0;
    write_prompt();
    write_str("/bin/process_stack_test\n");
    applets_ok &=
        run_program("/bin/process_stack_test", "process_stack_test") == 0;
    write_prompt();
    write_str("/bin/file_api_test\n");
    applets_ok &= run_program("/bin/file_api_test", "file_api_test") == 0;
    write_str("/bin/persistence_test\n");
    applets_ok &= run_program("/bin/persistence_test", "persistence_test") == 0;
    write_prompt();
    write_str("/bin/nasm_test\n");
    applets_ok &= run_program("/bin/nasm_test", "nasm_test") == 0;
    applets_ok &= run_startup_command("/bin/vix_hello --vix-smoke") == 0;
    write_prompt();
    write_str("/bin/vix_runtime_test\n");
    applets_ok &= run_program("/bin/vix_runtime_test", "vix_runtime_test") == 0;
    write_prompt();
    write_str("/bin/vixc_test\n");
    applets_ok &= run_program("/bin/vixc_test", "vixc_test") == 0;
    if (run_startup_command("ed --visual-smoke") == 0 &&
        run_startup_command("cat /work/visual-smoke") == 0) {
      write_str("[ed_visual_driver_test] PASS\n");
    } else {
      applets_ok = 0;
      write_str("[ed_visual_driver_test] FAIL\n");
    }
    if (run_startup_command("touch /work/touched-a /work/touched-b") == 0 &&
        run_startup_command("wc -c /work/touched-a") == 0) {
      write_str("[touch_test] PASS\n");
    } else {
      applets_ok = 0;
      write_str("[touch_test] FAIL\n");
    }
    if (run_startup_command(
            "printf 'a\\none\\ntwo\\n.\\n1i\\nzero\\n.\\n2d\\nw "
            "/work/ed-ops\\nq\\n' | ed") == 0 &&
        run_startup_command("cat /work/ed-ops") == 0 &&
        run_startup_command("printf '1p\\nq\\n' | ed /work/ed-ops") == 0 &&
        run_startup_command("printf 'a\\nscratch\\n.\\nq\\nQ\\n' | ed") == 0 &&
        run_startup_command(
            "printf 'a\\nfn main():i32{print(\"[ed_vix_test] PASS\") return "
            "0}\\n.\\nw /work/edited.vix\\nq\\n' | ed") == 0 &&
        run_startup_command("vixc /work/edited.vix -o /work/edited") == 0 &&
        run_startup_command("/work/edited") == 0) {
      write_str("[ed_test] PASS\n");
    } else {
      applets_ok = 0;
      write_str("[ed_test] FAIL\n");
    }
    write_prompt();
    write_str("/bin/udp_test\n");
    applets_ok &= run_program("/bin/udp_test", "udp_test") == 0;
    applets_ok &= run_startup_command("toybox") == 0;
    applets_ok &= run_startup_command("echo AukOS ToyBox") == 0;
    applets_ok &= run_startup_command("echo \"quoted argument\"") == 0;
    applets_ok &= run_startup_command("pwd") == 0;
    if (run_startup_command("cd /ext4") == 0 &&
        run_startup_command("pwd") == 0 && run_startup_command("cd -") == 0 &&
        run_startup_command("pwd") == 0 && run_startup_command("cd") == 0 &&
        run_startup_command("pwd") == 0 &&
        run_startup_command("cd /bin") == 0 && run_startup_command("ls") == 0 &&
        run_startup_command("cd -") == 0 && run_startup_command("pwd") == 0) {
      write_str("[shell_test] cd PASS\n");
    } else {
      applets_ok = 0;
      write_str("[shell_test] cd FAIL\n");
    }
    applets_ok &= run_startup_command("ls /ext4") == 0;
    if (run_startup_command("mkdir /tmp/empty-ls-test") == 0 &&
        run_startup_command("cd /tmp/empty-ls-test") == 0 &&
        run_startup_command("ls") == 0 &&
        run_startup_command("cd /") == 0 &&
        run_startup_command("rmdir /tmp/empty-ls-test") == 0) {
      write_str("[empty_directory_ls_test] PASS\n");
    } else {
      applets_ok = 0;
      write_str("[empty_directory_ls_test] FAIL\n");
    }
    if (run_startup_command("ls -1d /bin /ext4/hello.txt /bin/toybox") == 0) {
      write_str("[ls_color_test] PASS\n");
    } else {
      applets_ok = 0;
      write_str("[ls_color_test] FAIL\n");
    }
    applets_ok &= run_startup_command("cat /ext4/hello.txt") == 0;
    applets_ok &= run_startup_command("uname -a") == 0;
    true_status = run_startup_command("true");
    false_status = run_startup_command("false");
    if (true_status == 0 && false_status != 0) {
      write_str("[toybox_test] true/false exit status PASS\n");
    } else {
      applets_ok = 0;
      write_str("[toybox_test] true/false exit status FAIL\n");
    }
    applets_ok &= run_startup_command("basename /ext4/hello.txt") == 0;
    applets_ok &= run_startup_command("dirname /ext4/hello.txt") == 0;
    applets_ok &= run_startup_command("head -n 1 /ext4/hello.txt") == 0;
    applets_ok &= run_startup_command("wc -c /ext4/hello.txt") == 0;
    applets_ok &= run_startup_command("printf '%s\\n' AukOS") == 0;
    if (run_startup_command("printf 'pipeline-ok\\n' | wc -c") == 0) {
      write_str("[shell_test] pipeline PASS\n");
    } else {
      applets_ok = 0;
      write_str("[shell_test] pipeline FAIL\n");
    }
    if (run_startup_command("echo first > /work/output") == 0 &&
        run_startup_command("echo second >> /work/output") == 0 &&
        run_startup_command("cat < /work/output") == 0) {
      write_str("[shell_test] redirection PASS\n");
    } else {
      applets_ok = 0;
      write_str("[shell_test] redirection FAIL\n");
    }
    write_str(applets_ok ? "[toybox_test] default command dispatch PASS\n"
                         : "[toybox_test] default command dispatch FAIL\n");
  }

  for (;;) {
    uint64_t count;

    write_prompt();
    count = syscall3(SYS_READ, 0, (uint64_t)line, sizeof(line) - 1);
    if (count == UINT64_MAX_VALUE || count == 0) {
      write_str("\n");
      continue;
    }
    line[count] = '\0';
    strip_newline(line);
    run_command(line, sizeof(line));
  }
}
