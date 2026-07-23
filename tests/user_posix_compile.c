#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <langinfo.h>
#include <locale.h>
#include <arpa/inet.h>
#include <math.h>
#include <regex.h>
#include <poll.h>
#include <signal.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

int main(void)
{
    struct stat st;
    struct dirent64 dirent;
    DIR *dir;
    struct winsize ws;
    struct timeval tv;
    struct timespec ts;
    struct sigaction sa;
    struct utsname uts;
    struct termios tio;
    struct pollfd pfd;
    fd_set fds;
    struct sockaddr sa_in = {0};
    regex_t regex;
    regmatch_t match;
    struct rlimit rlim;
    struct rusage usage;
    struct statfs sfs;
    socklen_t salen = sizeof(sa_in);
    char buffer[32];
    int pipefd[2];
    char *const argv[] = {"/bin/sh", 0};
    char *const envp[] = {0};

    (void)open("/", O_RDONLY);
    (void)openat(AT_FDCWD, "/", O_RDONLY);
    (void)read(0, &dirent, sizeof(dirent));
    (void)write(1, "", 0);
    (void)lseek(0, 0, SEEK_SET);
    (void)stat("/", &st);
    (void)fstat(0, &st);
    (void)fstatat(AT_FDCWD, "/", &st, 0);
    (void)mkdir("/tmp", 0755);
    (void)unlink("/tmp/file");
    (void)getdents64(0, &dirent, sizeof(dirent));
    dir = opendir("/");
    (void)readdir64(dir);
    (void)closedir(dir);
    (void)getcwd(buffer, sizeof(buffer));
    (void)readlinkat(AT_FDCWD, "/bin/sh", buffer, sizeof(buffer));
    (void)getpid();
    (void)dup2(1, 2);
    (void)fork();
    (void)execve("/bin/sh", argv, envp);
    (void)waitpid(1, 0, WNOHANG);
    (void)pipe(pipefd);
    (void)isatty(1);
    (void)ioctl(1, TIOCGWINSZ, &ws);
    (void)memset(buffer, 0, sizeof(buffer));
    (void)strlen(buffer);
    (void)malloc(16);
    (void)printf("%s %d\n", "ok", 1);
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    (void)gettimeofday(&tv, 0);
    (void)nanosleep(&ts, 0);
    (void)time(0);
    (void)localtime(0);
    (void)strftime(buffer, sizeof(buffer), "%H:%M", 0);
    (void)uname(&uts);
    (void)signal(SIGINT, SIG_IGN);
    (void)sigemptyset(&sa.sa_mask);
    (void)sigaddset(&sa.sa_mask, SIGTERM);
    (void)sigaction(SIGINT, &sa, 0);
    (void)kill(1, SIGTERM);
    (void)setlocale(LC_ALL, "C");
    (void)nl_langinfo(CODESET);
    (void)tcgetattr(0, &tio);
    (void)tcsetattr(0, TCSANOW, &tio);
    pfd.fd = 0;
    pfd.events = POLLIN;
    (void)poll(&pfd, 1, 0);
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    (void)FD_ISSET(0, &fds);
    (void)select(1, &fds, &fds, 0, &tv);
    (void)socket(AF_INET, SOCK_STREAM, 0);
    (void)connect(0, &sa_in, salen);
    (void)bind(0, &sa_in, salen);
    (void)listen(0, 1);
    (void)accept(0, &sa_in, &salen);
    (void)sendto(0, buffer, sizeof(buffer), 0, &sa_in, salen);
    (void)recvfrom(0, buffer, sizeof(buffer), 0, &sa_in, &salen);
    (void)regcomp(&regex, "a", REG_EXTENDED);
    (void)regexec(&regex, "b", 1, &match, 0);
    (void)regerror(REG_NOMATCH, &regex, buffer, sizeof(buffer));
    regfree(&regex);
    (void)fabs(-1.0);
    (void)floor(1.5);
    (void)ceil(1.5);
    (void)log(1.0);
    (void)pow(2.0, 3.0);
    (void)sched_yield();
    (void)getrlimit(RLIMIT_NOFILE, &rlim);
    (void)setrlimit(RLIMIT_NOFILE, &rlim);
    (void)getrusage(RUSAGE_SELF, &usage);
    (void)statfs("/", &sfs);
    (void)fstatfs(0, &sfs);
    (void)htons(1);
    (void)htonl(1);
    (void)inet_pton(AF_INET, "127.0.0.1", buffer);
    (void)inet_ntop(AF_INET, buffer, buffer, sizeof(buffer));
    (void)close(0);
    return 0;
}
