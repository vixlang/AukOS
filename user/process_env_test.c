#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define EXEC_ARGC 40
#define LONG_ARG_SIZE 512u

static int child_checks(int argc, char **argv)
{
    char cwd[256];
    char data[5];
    int fd;

    if (argc != EXEC_ARGC || strcmp(argv[1], "--exec-child") != 0 ||
        strlen(argv[2]) != LONG_ARG_SIZE || strlen(argv[EXEC_ARGC - 1]) != LONG_ARG_SIZE ||
        !getenv("AUKOS_ENV_TEST") || strcmp(getenv("AUKOS_ENV_TEST"), "through-exec") != 0 ||
        !getcwd(cwd, sizeof(cwd)) || strcmp(cwd, "/work") != 0) {
        return 2;
    }
    fd = open("relative.txt", O_RDONLY);
    if (fd < 0 || read(fd, data, sizeof(data)) != (ssize_t)sizeof(data) ||
        memcmp(data, "cwd-ok", sizeof(data)) != 0) {
        return 3;
    }
    close(fd);
    return 0;
}

int main(int argc, char **argv)
{
    char long_arg[LONG_ARG_SIZE + 1u];
    char *exec_argv[EXEC_ARGC + 1u];
    int fd;
    pid_t child;
    int status = 1;

    if (argc > 1 && strcmp(argv[1], "--exec-child") == 0) {
        return child_checks(argc, argv);
    }
    if (!getenv("PATH") || strcmp(getenv("PATH"), "/bin") != 0 ||
        setenv("REPLACE", "old", 1) != 0 ||
        setenv("REPLACE", "new", 1) != 0 ||
        strcmp(getenv("REPLACE"), "new") != 0 ||
        setenv("REMOVE", "yes", 1) != 0 || unsetenv("REMOVE") != 0 ||
        getenv("REMOVE") != 0 || setenv("AUKOS_ENV_TEST", "through-exec", 1) != 0) {
        puts("[process_env_test] FAIL environment");
        return 1;
    }
    if (chdir("/work") != 0) {
        puts("[process_env_test] FAIL chdir");
        return 1;
    }
    fd = open("./relative.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0 || write(fd, "cwd-ok", 5u) != 5) {
        puts("[process_env_test] FAIL relative create");
        return 1;
    }
    close(fd);

    memset(long_arg, 'L', LONG_ARG_SIZE);
    long_arg[LONG_ARG_SIZE] = '\0';
    exec_argv[0] = "process_env_test";
    exec_argv[1] = "--exec-child";
    for (int index = 2; index < EXEC_ARGC; index++) {
        exec_argv[index] = long_arg;
    }
    exec_argv[EXEC_ARGC] = 0;

    child = fork();
    if (child == 0) {
        execvp("process_env_test", exec_argv);
        _exit(127);
    }
    if (child < 0 || waitpid(child, &status, 0) != child || status != 0) {
        puts("[process_env_test] FAIL fork/exec/env/argv/cwd");
        return 1;
    }
    {
        char *missing_argv[] = {"definitely-missing", 0};
        if (execvp("definitely-missing", missing_argv) != -1 ||
            strcmp(getenv("AUKOS_ENV_TEST"), "through-exec") != 0) {
            puts("[process_env_test] FAIL PATH miss");
            return 1;
        }
    }
    puts("[process_env_test] PASS");
    return 0;
}
