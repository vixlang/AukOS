#include "include/aukos/syscall.h"

#include "include/aukos/block.h"
#include "include/aukos/console.h"
#include "include/aukos/descriptor.h"
#include "include/aukos/elf.h"
#include "include/aukos/ext4.h"
#include "include/aukos/fat32.h"
#include "include/aukos/keyboard.h"
#include "include/aukos/log.h"
#include "include/aukos/memory.h"
#include "include/aukos/net.h"
#include "include/aukos/net_packets.h"
#include "include/aukos/pipe.h"
#include "include/aukos/scheduler.h"
#include "include/aukos/serial.h"
#include "include/aukos/task.h"
#include "include/aukos/timer.h"
#include "include/aukos/vfs.h"
#include "include/aukos/vmm.h"

#include <stdint.h>

#define MSR_EFER 0xc0000080u
#define MSR_STAR 0xc0000081u
#define MSR_LSTAR 0xc0000082u
#define MSR_SFMASK 0xc0000084u
#define EFER_SCE 0x1u
#define KERNEL_CODE_SELECTOR 0x08u
#define USER_CODE_SELECTOR 0x20u
#define USER_DATA_SELECTOR 0x18u
#define USER_SYSCALL_STAR_SELECTOR (USER_CODE_SELECTOR - 0x10u)
#define RFLAGS_INTERRUPT_ENABLE 0x200u
#define USER_STACK_TOP VMM_USER_STACK_TOP
#define EXEC_MAX_ARGS 64u
#define EXEC_MAX_ENV 64u
#define EXEC_MAX_STRING_LENGTH 4096u
#define EXEC_MAX_STACK_DATA (32u * 1024u)
#define SIGNAL_HANDLER_DEFAULT 0ull
#define SIGNAL_HANDLER_IGNORE 1ull
#define SIGNAL_SIGKILL 9u
#define SIGNAL_SIGSEGV 11u
#define SIGNAL_SIGCHLD 17u
#define SIGNAL_SIGCONT 18u
#define SIGNAL_SIGSTOP 19u
#define SIGNAL_SIGURG 23u
#define SIGNAL_SIGWINCH 28u
#define SIGNAL_BLOCK 0u
#define SIGNAL_UNBLOCK 1u
#define SIGNAL_SETMASK 2u
#define SIGNAL_FLAG_RESTORER 0x04000000ull
#define SIGNAL_FLAG_NODEFER 0x40000000ull
#define SIGNAL_FLAG_RESETHAND 0x80000000ull
#define SYSCALL_PROT_READ 0x1u
#define SYSCALL_PROT_WRITE 0x2u
#define SYSCALL_PROT_EXEC 0x4u
#define SYSCALL_MAP_PRIVATE 0x2u
#define SYSCALL_MAP_ANONYMOUS 0x20u
#define SYSCALL_WNOHANG 0x1u
#define SYSCALL_TCGETS 0x5401u
#define SYSCALL_TCSETS 0x5402u
#define SYSCALL_TIOCGWINSZ 0x5413u
#define SYSCALL_TTY_ICANON 0x0002u
#define SYSCALL_TTY_ECHO 0x0008u

struct syscall_termios {
    uint32_t iflag;
    uint32_t oflag;
    uint32_t cflag;
    uint32_t lflag;
    uint8_t cc[32];
};

struct syscall_winsize {
    uint16_t rows;
    uint16_t columns;
    uint16_t x_pixels;
    uint16_t y_pixels;
};

extern void x86_64_syscall_entry(void);
extern void x86_64_enter_user(uintptr_t entry, uintptr_t stack_top, uintptr_t pml4, uintptr_t argc, uintptr_t argv, uintptr_t envp) __attribute__((noreturn));
extern void x86_64_resume_user(struct user_context *ctx) __attribute__((noreturn));
extern void x86_64_switch_and_resume(uintptr_t pml4, struct user_context *ctx) __attribute__((noreturn));
extern void x86_64_switch_exec(uintptr_t pml4) __attribute__((noreturn));

static uint64_t random_state = 0x41756b4f53524e47ull;

struct exec_commit_state {
    struct address_space retired_space;
    uintptr_t entry;
    uintptr_t stack_top;
    uintptr_t pml4;
    uintptr_t argc;
    uintptr_t argv;
    uintptr_t envp;
};

static struct exec_commit_state exec_commit;

/* Global CR3 value to switch on return to user mode */
uint64_t syscall_next_cr3 = 0;

struct syscall_fsinfo {
    uint32_t tmpfs_ready;
    uint32_t ext4_ready;
    uint32_t fat32_ready;
    uint32_t block_devices;
};

struct syscall_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[VFS_MAX_NAME];
};

struct syscall_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

struct exec_stack {
    uint64_t argc;
    uintptr_t argv;
    uintptr_t envp;
    uintptr_t stack_top;
};

struct syscall_sigaction {
    uintptr_t handler;
    uint64_t mask;
    uint64_t flags;
    uintptr_t restorer;
};

struct syscall_sockaddr_in {
    uint16_t family;
    uint16_t port;
    uint8_t address[NET_IPV4_ADDRESS_SIZE];
    uint8_t zero[8];
};

_Static_assert(sizeof(struct syscall_sockaddr_in) == 16u,
               "IPv4 socket address ABI size");

#define SYSCALL_AF_INET 2u
#define SYSCALL_SOCK_STREAM 1u
#define SYSCALL_SOCK_DGRAM 2u
#define SYSCALL_IPPROTO_TCP 6u
#define SYSCALL_IPPROTO_UDP 17u

static void copy_string(char *dest, const char *src, size_t max)
{
    size_t i;
    for (i = 0; i + 1 < max && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static void memory_copy(uint8_t *dest, const uint8_t *src, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        dest[i] = src[i];
    }
}

static void memory_set(uint8_t *dest, uint8_t value, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        dest[i] = value;
    }
}

static uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1);
}

static int map_user_signal_stacks(struct address_space *space)
{
    return vmm_map_anonymous_region(
        space, VMM_SIGNAL_STACK_BASE,
        VMM_SIGNAL_STACK_PAGES * PAGE_SIZE,
        VMM_USER_FLAG_READABLE | VMM_USER_FLAG_WRITABLE | VMM_USER_FLAG_USER,
        VM_REGION_SIGNAL_STACK);
}

static uint64_t signal_bit(uint32_t signal)
{
    return 1ull << signal;
}

static uint64_t sanitise_signal_mask(uint64_t mask)
{
    return mask & ~signal_bit(SIGNAL_SIGKILL) & ~signal_bit(SIGNAL_SIGSTOP);
}

static size_t user_string_length(const char *value, size_t max_length)
{
    size_t length = 0;

    while (length < max_length && value[length] != '\0') {
        length++;
    }

    return length;
}

static int exec_stack_write(struct address_space *space, uintptr_t address,
                            const void *source, size_t size)
{
    const uint8_t *bytes = source;

    while (size != 0u) {
        uintptr_t page = address & ~(PAGE_SIZE - 1u);
        uintptr_t physical;
        size_t offset = address - page;
        size_t chunk = PAGE_SIZE - offset;

        if (chunk > size) {
            chunk = size;
        }
        if (vmm_query_page(space, page, &physical) != 0) {
            return -1;
        }
        memory_copy((uint8_t *)physical + offset, bytes, chunk);
        bytes += chunk;
        address += chunk;
        size -= chunk;
    }
    return 0;
}

static int exec_stack_write_u64(struct address_space *space,
                                uintptr_t address, uint64_t value)
{
    return exec_stack_write(space, address, &value, sizeof(value));
}

static int build_exec_stack(struct address_space *space, const char *path,
                            uint64_t argv_address, uint64_t envp_address,
                            struct exec_stack *stack)
{
    static const char *const default_env[] = {
        "PATH=/bin", "HOME=/", "TMPDIR=/tmp", 0
    };
    const char *arg_strings[EXEC_MAX_ARGS];
    const char *env_strings[EXEC_MAX_ENV];
    uintptr_t arg_virtuals[EXEC_MAX_ARGS];
    uintptr_t env_virtuals[EXEC_MAX_ENV];
    uint64_t argc = 0;
    uint64_t envc = 0;
    uintptr_t stack_base = VMM_EXEC_STACK_INITIAL_BASE;
    uintptr_t cursor = USER_STACK_TOP;
    const char *const *argv = (const char *const *)(uintptr_t)argv_address;
    const char *const *envp = (const char *const *)(uintptr_t)envp_address;

    if (argv) {
        while (argc < EXEC_MAX_ARGS && argv[argc]) {
            arg_strings[argc] = argv[argc];
            argc++;
        }
        if (argc == EXEC_MAX_ARGS && argv[argc]) {
            return -1;
        }
    }
    if (argc == 0u) {
        arg_strings[argc++] = path;
    }
    if (envp) {
        while (envc < EXEC_MAX_ENV && envp[envc]) {
            env_strings[envc] = envp[envc];
            envc++;
        }
        if (envc == EXEC_MAX_ENV && envp[envc]) {
            return -1;
        }
    } else {
        while (default_env[envc]) {
            env_strings[envc] = default_env[envc];
            envc++;
        }
    }

    for (uint64_t index = envc; index > 0u; index--) {
        uint64_t item = index - 1u;
        size_t length = user_string_length(env_strings[item],
                                           EXEC_MAX_STRING_LENGTH);

        if (length == EXEC_MAX_STRING_LENGTH ||
            cursor < stack_base + length + 1u) {
            return -1;
        }
        cursor -= length + 1u;
        if (exec_stack_write(space, cursor, env_strings[item], length + 1u) != 0) {
            return -1;
        }
        env_virtuals[item] = cursor;
    }
    for (uint64_t index = argc; index > 0u; index--) {
        uint64_t item = index - 1u;
        size_t length = user_string_length(arg_strings[item],
                                           EXEC_MAX_STRING_LENGTH);

        if (length == EXEC_MAX_STRING_LENGTH ||
            cursor < stack_base + length + 1u) {
            return -1;
        }
        cursor -= length + 1u;
        if (exec_stack_write(space, cursor, arg_strings[item], length + 1u) != 0) {
            return -1;
        }
        arg_virtuals[item] = cursor;
    }

    cursor = align_down(cursor, 16);
    if (USER_STACK_TOP - cursor > EXEC_MAX_STACK_DATA ||
        cursor < stack_base + ((envc + 1u) * sizeof(uint64_t))) {
        return -1;
    }
    cursor -= (envc + 1u) * sizeof(uint64_t);
    stack->envp = cursor;
    for (uint64_t index = 0; index < envc; index++) {
        if (exec_stack_write_u64(space, cursor + index * sizeof(uint64_t),
                                 env_virtuals[index]) != 0) {
            return -1;
        }
    }
    if (exec_stack_write_u64(space, cursor + envc * sizeof(uint64_t), 0) != 0 ||
        cursor < stack_base + ((argc + 1u) * sizeof(uint64_t)) + 15u) {
        return -1;
    }
    cursor = align_down(cursor - ((argc + 1u) * sizeof(uint64_t)), 16);
    stack->argv = cursor;
    for (uint64_t index = 0; index < argc; index++) {
        if (exec_stack_write_u64(space, cursor + index * sizeof(uint64_t),
                                 arg_virtuals[index]) != 0) {
            return -1;
        }
    }
    if (exec_stack_write_u64(space, cursor + argc * sizeof(uint64_t), 0) != 0) {
        return -1;
    }
    stack->argc = argc;
    if (cursor < stack_base + sizeof(uint64_t)) {
        return -1;
    }
    cursor -= sizeof(uint64_t);
    if (exec_stack_write_u64(space, cursor, 0) != 0) {
        return -1;
    }
    stack->stack_top = cursor;
    return 0;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    memory_copy((uint8_t *)dest, (const uint8_t *)src, n);
    return dest;
}

void *memset(void *dest, int c, size_t n)
{
    memory_set((uint8_t *)dest, (uint8_t)c, n);
    return dest;
}

static uint64_t rdmsr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);

    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static uint64_t syscall_write(struct user_context *ctx, uint64_t fd,
                              uint64_t buffer_address, uint64_t length)
{
    const char *message = (const char *)buffer_address;
    struct vfs_file *file;
    struct pipe_object *pipe;
    struct tcp_socket *tcp;
    enum pipe_io_result pipe_result;
    size_t transferred;
    int standard_fd;
    int write_end;

    if (!current_process || fd >= MAX_FD || !current_process->files[fd]) {
        return UINT64_MAX;
    }
    standard_fd = descriptor_standard_fd(current_process->files[fd]);
    if (standard_fd == 1 || standard_fd == 2) {
        for (uint64_t index = 0; index < length; index++) {
            serial_write((char[]){ message[index], '\0' });
            console_put_char(message[index]);
        }
        return length;
    }

    pipe = descriptor_pipe(current_process->files[fd], &write_end);
    if (pipe) {
        if (!write_end) {
            return UINT64_MAX;
        }
        pipe_result = pipe_write(pipe, message, (size_t)length, &transferred);
        if (pipe_result == PIPE_IO_OK) {
            scheduler_pipe_changed(pipe);
            return transferred;
        }
        if (pipe_result == PIPE_IO_WOULD_BLOCK) {
            (void)scheduler_block_current(ctx, PROCESS_WAIT_PIPE_WRITE, 0,
                                          pipe, SYS_WRITE);
        }
        return UINT64_MAX;
    }

    tcp = descriptor_tcp_socket(current_process->files[fd]);
    if (tcp) {
        if (net_tcp_socket_write(tcp, (const uint8_t *)buffer_address,
                                 (size_t)length, &transferred) != 0) {
            return UINT64_MAX;
        }
        return transferred;
    }

    if (!(file = descriptor_vfs_file(current_process->files[fd]))) {
        return UINT64_MAX;
    }

    return vfs_write(file, (const void *)buffer_address, (size_t)length);
}

static uint64_t syscall_read_stdin(uint64_t buffer_address, uint64_t length)
{
    char *buffer = (char *)buffer_address;
    uint64_t count = 0;
    int canonical = (current_process->tty_lflag & SYSCALL_TTY_ICANON) != 0;
    int echo = (current_process->tty_lflag & SYSCALL_TTY_ECHO) != 0;

    while (count < length) {
        uint8_t character = keyboard_read_char();

        if (canonical && character >= KEYBOARD_KEY_UP &&
            character <= KEYBOARD_KEY_RIGHT) {
            continue;
        }

        if (canonical && character == '\b') {
            if (count > 0) {
                count--;
                if (echo) {
                    serial_write("\b \b");
                    console_put_char('\b');
                }
            }
            continue;
        }

        buffer[count++] = (char)character;
        if (echo) {
            serial_write((char[]){ (char)character, '\0' });
            console_put_char((char)character);
        }

        if (!canonical || character == '\n') {
            break;
        }
    }

    return count;
}

static uint64_t syscall_ioctl(uint64_t fd, uint64_t request,
                              uint64_t argument)
{
    int standard_fd;

    if (!current_process || fd >= MAX_FD || !current_process->files[fd]) {
        return UINT64_MAX;
    }
    standard_fd = descriptor_standard_fd(current_process->files[fd]);
    if (standard_fd < 0 || argument == 0) {
        return UINT64_MAX;
    }
    if (request == SYSCALL_TIOCGWINSZ) {
        struct syscall_winsize *size = (struct syscall_winsize *)argument;

        size->rows = console_terminal_rows();
        size->columns = console_terminal_columns();
        size->x_pixels = console_width_pixels();
        size->y_pixels = console_height_pixels();
        return 0;
    }
    if (request == SYSCALL_TCGETS) {
        struct syscall_termios *termios = (struct syscall_termios *)argument;

        termios->iflag = 0;
        termios->oflag = 0;
        termios->cflag = 0;
        termios->lflag = current_process->tty_lflag;
        for (size_t index = 0; index < sizeof(termios->cc); index++) {
            termios->cc[index] = 0;
        }
        return 0;
    }
    if (request == SYSCALL_TCSETS) {
        const struct syscall_termios *termios =
            (const struct syscall_termios *)argument;

        current_process->tty_lflag = termios->lflag;
        return 0;
    }
    return UINT64_MAX;
}

static uint64_t syscall_random(void)
{
    random_state = random_state * 6364136223846793005ull + 1;
    return random_state;
}

static int resolve_process_path(const char *path, char resolved[VFS_MAX_PATH])
{
    if (!current_process) {
        return -1;
    }
    return vfs_resolve_path(current_process->cwd, path, resolved,
                            VFS_MAX_PATH);
}

static uint64_t syscall_open(uint64_t path_address, uint64_t flags, uint64_t mode)
{
    struct descriptor *descriptor;
    struct vfs_file *file;
    char path[VFS_MAX_PATH];

    if (!current_process ||
        resolve_process_path((const char *)path_address, path) != 0) {
        return UINT64_MAX;
    }

    for (uint32_t fd = 3; fd < MAX_FD; fd++) {
        if (!current_process->files[fd]) {
            if (vfs_open(path, (uint32_t)flags, (uint32_t)mode, &file) != 0) {
                return UINT64_MAX;
            }
            descriptor = descriptor_create_vfs(file);
            if (!descriptor) {
                if (file->created) {
                    (void)vfs_unlink(path);
                }
                vfs_close(file);
                return UINT64_MAX;
            }
            current_process->files[fd] = descriptor;
            return fd;
        }
    }

    return UINT64_MAX;
}

static uint64_t syscall_read(struct user_context *ctx, uint64_t fd,
                             uint64_t buffer_address, uint64_t length)
{
    struct vfs_file *file;
    struct pipe_object *pipe;
    struct tcp_socket *tcp;
    enum pipe_io_result pipe_result;
    size_t transferred;
    int standard_fd;
    int write_end;

    if (!current_process || fd >= MAX_FD || !current_process->files[fd]) {
        return UINT64_MAX;
    }
    standard_fd = descriptor_standard_fd(current_process->files[fd]);
    if (standard_fd == 0) {
        return syscall_read_stdin(buffer_address, length);
    }

    pipe = descriptor_pipe(current_process->files[fd], &write_end);
    if (pipe) {
        if (write_end) {
            return UINT64_MAX;
        }
        pipe_result = pipe_read(pipe, (void *)buffer_address, (size_t)length,
                                &transferred);
        if (pipe_result == PIPE_IO_OK || pipe_result == PIPE_IO_EOF) {
            scheduler_pipe_changed(pipe);
            return transferred;
        }
        if (pipe_result == PIPE_IO_WOULD_BLOCK) {
            (void)scheduler_block_current(ctx, PROCESS_WAIT_PIPE_READ, 0,
                                          pipe, SYS_READ);
        }
        return UINT64_MAX;
    }


    tcp = descriptor_tcp_socket(current_process->files[fd]);
    if (tcp) {
        if (net_tcp_socket_read(tcp, (uint8_t *)buffer_address,
                                (size_t)length, &transferred) != 0) {
            return UINT64_MAX;
        }
        return transferred;
    }

    if (!(file = descriptor_vfs_file(current_process->files[fd]))) {
        return UINT64_MAX;
    }

    return vfs_read(file, (void *)buffer_address, (size_t)length);
}

static uint64_t syscall_close(uint64_t fd)
{
    struct pipe_object *pipe;

    if (!current_process || fd >= MAX_FD || !current_process->files[fd]) {
        return UINT64_MAX;
    }

    pipe = descriptor_pipe(current_process->files[fd], 0);
    descriptor_release(current_process->files[fd]);
    current_process->files[fd] = 0;
    if (pipe) {
        scheduler_pipe_changed(pipe);
    }
    return 0;
}

static uint64_t syscall_mkdir(uint64_t path_address, uint64_t mode)
{
    char path[VFS_MAX_PATH];

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_mkdir(path, (uint32_t)mode) != 0) {
        return UINT64_MAX;
    }

    return 0;
}

static uint64_t syscall_unlink(uint64_t path_address)
{
    char path[VFS_MAX_PATH];

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_unlink(path) != 0) {
        return UINT64_MAX;
    }

    return 0;
}

static uint64_t syscall_rmdir(uint64_t path_address)
{
    char path[VFS_MAX_PATH];

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_rmdir(path) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_fsync(uint64_t fd)
{
    struct vfs_file *file;

    if (!current_process || fd >= MAX_FD ||
        !(file = descriptor_vfs_file(current_process->files[fd])) ||
        vfs_fsync(file) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_truncate(uint64_t path_address, uint64_t length)
{
    char path[VFS_MAX_PATH];

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_truncate(path, length) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_ftruncate(uint64_t fd, uint64_t length)
{
    struct vfs_file *file;

    if (!current_process || fd >= MAX_FD ||
        !(file = descriptor_vfs_file(current_process->files[fd])) ||
        vfs_ftruncate(file, length) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_rename(uint64_t old_address, uint64_t new_address)
{
    char old_path[VFS_MAX_PATH];
    char new_path[VFS_MAX_PATH];

    if (resolve_process_path((const char *)old_address, old_path) != 0 ||
        resolve_process_path((const char *)new_address, new_path) != 0 ||
        vfs_rename(old_path, new_path) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_readdir(uint64_t fd, uint64_t buffer_address, uint64_t length)
{
    struct vfs_file *file;

    if (!current_process || fd >= MAX_FD ||
        !(file = descriptor_vfs_file(current_process->files[fd]))) {
        return UINT64_MAX;
    }

    return (uint64_t)vfs_readdir(file, (void *)buffer_address, (size_t)length);
}

static uint64_t syscall_fsinfo(uint64_t buffer_address, uint64_t length)
{
    struct syscall_fsinfo *info = (struct syscall_fsinfo *)buffer_address;

    if (!info || length < sizeof(struct syscall_fsinfo)) {
        return UINT64_MAX;
    }

    info->tmpfs_ready = 1;
    info->ext4_ready = ext4_available() ? 1u : 0u;
    info->fat32_ready = fat32_available() ? 1u : 0u;
    info->block_devices = block_count();
    return 0;
}

static uint64_t syscall_lseek(uint64_t fd, uint64_t offset, uint64_t whence)
{
    struct vfs_file *file;
    uint64_t new_offset;

    if (!current_process || fd >= MAX_FD ||
        !(file = descriptor_vfs_file(current_process->files[fd]))) {
        return UINT64_MAX;
    }

    if (vfs_seek(file, (int64_t)offset, (uint32_t)whence, &new_offset) != 0) {
        return UINT64_MAX;
    }

    return new_offset;
}

static uint64_t syscall_stat(uint64_t path_address, uint64_t stat_address)
{
    char path[VFS_MAX_PATH];

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_stat_path(path, (struct vfs_stat *)stat_address) != 0) {
        return UINT64_MAX;
    }

    return 0;
}

static uint64_t syscall_fstat(uint64_t fd, uint64_t stat_address)
{
    struct vfs_file *file;

    if (!current_process || fd >= MAX_FD ||
        !(file = descriptor_vfs_file(current_process->files[fd]))) {
        return UINT64_MAX;
    }

    if (vfs_stat_inode(file->inode, (struct vfs_stat *)stat_address) != 0) {
        return UINT64_MAX;
    }

    return 0;
}

static uint8_t dirent_type(enum vfs_file_type type)
{
    switch (type) {
    case VFS_FILE_REGULAR:
        return 8;
    case VFS_FILE_DIRECTORY:
        return 4;
    case VFS_FILE_SYMLINK:
        return 10;
    case VFS_FILE_DEVICE:
        return 6;
    case VFS_FILE_UNKNOWN:
    default:
        return 0;
    }
}

static void copy_name(char *dest, const char *src)
{
    size_t i;

    for (i = 0; i < VFS_MAX_NAME - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static uint64_t syscall_getdents64(uint64_t fd, uint64_t buffer_address, uint64_t length)
{
    struct vfs_dirent entries[16];
    struct syscall_dirent64 *out = (struct syscall_dirent64 *)buffer_address;
    size_t max_out = length / sizeof(struct syscall_dirent64);
    int count;
    struct vfs_file *file;

    if (!current_process || fd >= VFS_MAX_FD || !out || max_out == 0 ||
        !(file = descriptor_vfs_file(current_process->files[fd]))) {
        return UINT64_MAX;
    }

    count = vfs_readdir(file, entries, sizeof(entries));
    if (count < 0) {
        return UINT64_MAX;
    }
    if ((size_t)count > max_out) {
        count = (int)max_out;
    }

    for (int i = 0; i < count; i++) {
        out[i].d_ino = (uint64_t)i + 1;
        out[i].d_off = i + 1;
        out[i].d_reclen = sizeof(struct syscall_dirent64);
        out[i].d_type = dirent_type(entries[i].type);
        copy_name(out[i].d_name, entries[i].name);
    }

    return (uint64_t)count * sizeof(struct syscall_dirent64);
}

static uint64_t syscall_getcwd(uint64_t buffer_address, uint64_t length)
{
    char *buffer = (char *)buffer_address;
    size_t cwd_length;

    if (!current_process || !buffer) {
        return UINT64_MAX;
    }
    cwd_length = user_string_length(current_process->cwd,
                                    sizeof(current_process->cwd)) + 1u;
    if (length < cwd_length) {
        return UINT64_MAX;
    }
    memory_copy((uint8_t *)buffer, (const uint8_t *)current_process->cwd,
                cwd_length);
    return cwd_length;
}

static uint64_t syscall_chdir(uint64_t path_address)
{
    char path[VFS_MAX_PATH];
    struct vfs_inode *inode;

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_lookup(path, &inode) != 0 ||
        inode->type != VFS_FILE_DIRECTORY) {
        return UINT64_MAX;
    }
    copy_string(current_process->cwd, path, sizeof(current_process->cwd));
    return 0;
}

static uint64_t syscall_readlinkat(uint64_t dirfd, uint64_t path_address, uint64_t buffer_address, uint64_t length)
{
    (void)dirfd;
    (void)path_address;
    (void)buffer_address;
    (void)length;
    return UINT64_MAX;
}

static uint64_t syscall_getpid(void)
{
    if (current_process) {
        return current_process->pid;
    }
    return 1;
}

static uint64_t syscall_dup2(uint64_t oldfd, uint64_t newfd)
{
    struct pipe_object *replaced_pipe = 0;

    if (!current_process || oldfd >= VFS_MAX_FD || newfd >= VFS_MAX_FD) {
        return UINT64_MAX;
    }
    if (!current_process->files[oldfd]) {
        return UINT64_MAX;
    }
    if (oldfd == newfd) {
        return newfd;
    }
    if (current_process->files[newfd]) {
        replaced_pipe = descriptor_pipe(current_process->files[newfd], 0);
        descriptor_release(current_process->files[newfd]);
        current_process->files[newfd] = 0;
        if (replaced_pipe) {
            scheduler_pipe_changed(replaced_pipe);
        }
    }
    if (descriptor_retain(current_process->files[oldfd]) != 0) {
        return UINT64_MAX;
    }
    current_process->files[newfd] = current_process->files[oldfd];
    return newfd;
}

static uint64_t syscall_dup(uint64_t oldfd)
{
    if (!current_process || oldfd >= VFS_MAX_FD) {
        return UINT64_MAX;
    }
    if (!current_process->files[oldfd]) {
        return UINT64_MAX;
    }

    for (uint32_t newfd = 0; newfd < VFS_MAX_FD; newfd++) {
        if (!current_process->files[newfd]) {
            if (descriptor_retain(current_process->files[oldfd]) != 0) {
                return UINT64_MAX;
            }
            current_process->files[newfd] = current_process->files[oldfd];
            return newfd;
        }
    }

    return UINT64_MAX;
}

static void syscall_uname(uint64_t buffer_address)
{
    struct syscall_utsname *name = (struct syscall_utsname *)buffer_address;

    if (!name) {
        return;
    }

    copy_string(name->sysname, "AukOS", sizeof(name->sysname));
    copy_string(name->nodename, "aukos", sizeof(name->nodename));
    copy_string(name->release, "0.0.1", sizeof(name->release));
    copy_string(name->version, "toybox-port", sizeof(name->version));
    copy_string(name->machine, "x86_64", sizeof(name->machine));
}

static uint64_t syscall_pipe(uint64_t pipefd_address)
{
    int *pipefd = (int *)(uintptr_t)pipefd_address;
    struct pipe_object *pipe;
    struct descriptor *read_descriptor;
    struct descriptor *write_descriptor;
    int read_fd = -1;
    int write_fd = -1;

    if (!current_process || !pipefd) {
        return UINT64_MAX;
    }
    for (int fd = 0; fd < MAX_FD; fd++) {
        if (!current_process->files[fd]) {
            if (read_fd < 0) {
                read_fd = fd;
            } else {
                write_fd = fd;
                break;
            }
        }
    }
    if (write_fd < 0 || !(pipe = pipe_allocate())) {
        return UINT64_MAX;
    }
    read_descriptor = descriptor_create_pipe(pipe, 0);
    write_descriptor = descriptor_create_pipe(pipe, 1);
    if (!read_descriptor || !write_descriptor) {
        descriptor_release(read_descriptor);
        descriptor_release(write_descriptor);
        pipe_endpoint_close(pipe, PIPE_READ_END);
        return UINT64_MAX;
    }
    current_process->files[read_fd] = read_descriptor;
    current_process->files[write_fd] = write_descriptor;
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return 0;
}

static uint64_t syscall_socket(uint64_t domain, uint64_t type,
                               uint64_t protocol)
{
    struct udp_socket *udp;
    struct tcp_socket *tcp;
    struct descriptor *descriptor;
    uint32_t fd;

    if (!current_process || domain != SYSCALL_AF_INET ||
        (type != SYSCALL_SOCK_DGRAM && type != SYSCALL_SOCK_STREAM) ||
        (type == SYSCALL_SOCK_DGRAM && protocol != 0u &&
         protocol != SYSCALL_IPPROTO_UDP) ||
        (type == SYSCALL_SOCK_STREAM && protocol != 0u &&
         protocol != SYSCALL_IPPROTO_TCP)) {
        return UINT64_MAX;
    }
    for (fd = 3u; fd < MAX_FD; fd++) {
        if (!current_process->files[fd]) {
            break;
        }
    }
    if (fd == MAX_FD) {
        return UINT64_MAX;
    }
    if (type == SYSCALL_SOCK_STREAM) {
        tcp = net_tcp_socket_create();
        descriptor = descriptor_create_tcp(tcp);
        if (!descriptor) net_tcp_socket_close(tcp);
    } else {
        udp = net_udp_socket_create();
        descriptor = descriptor_create_udp(udp);
        if (!descriptor) net_udp_socket_close(udp);
    }
    if (!descriptor) {
        return UINT64_MAX;
    }
    current_process->files[fd] = descriptor;
    return fd;
}

static uint64_t syscall_connect(uint64_t fd, uint64_t address_value,
                                uint64_t address_length)
{
    const struct syscall_sockaddr_in *address =
        (const struct syscall_sockaddr_in *)(uintptr_t)address_value;
    struct tcp_socket *socket;

    if (!current_process || fd >= MAX_FD || !address ||
        address_length < sizeof(*address) ||
        address->family != SYSCALL_AF_INET ||
        !(socket = descriptor_tcp_socket(current_process->files[fd])) ||
        net_tcp_socket_connect(socket, address->address,
                               net_load_be16((const uint8_t *)&address->port)) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_bind(uint64_t fd, uint64_t address_value,
                             uint64_t address_length)
{
    const struct syscall_sockaddr_in *address =
        (const struct syscall_sockaddr_in *)(uintptr_t)address_value;
    struct udp_socket *socket;

    if (!current_process || fd >= MAX_FD || !address ||
        address_length < sizeof(*address) ||
        address->family != SYSCALL_AF_INET ||
        !(socket = descriptor_udp_socket(current_process->files[fd])) ||
        net_udp_socket_bind(socket, address->address,
                            net_load_be16((const uint8_t *)&address->port)) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_sendto(uint64_t fd, uint64_t buffer_value,
                               uint64_t length, uint64_t flags,
                               uint64_t destination_value,
                               uint64_t destination_length)
{
    const struct syscall_sockaddr_in *destination =
        (const struct syscall_sockaddr_in *)(uintptr_t)destination_value;
    struct udp_socket *socket;
    size_t sent_length;

    if (!current_process || fd >= MAX_FD || !buffer_value || flags != 0u ||
        !destination || destination_length < sizeof(*destination) ||
        destination->family != SYSCALL_AF_INET || length > SIZE_MAX ||
        !(socket = descriptor_udp_socket(current_process->files[fd])) ||
        net_udp_socket_sendto(
            socket, (const uint8_t *)(uintptr_t)buffer_value, (size_t)length,
            destination->address,
            net_load_be16((const uint8_t *)&destination->port),
            &sent_length) != 0) {
        return UINT64_MAX;
    }
    return sent_length;
}

static uint64_t syscall_recvfrom(uint64_t fd, uint64_t buffer_value,
                                 uint64_t length, uint64_t flags,
                                 uint64_t source_value,
                                 uint64_t address_length_value)
{
    struct syscall_sockaddr_in *source =
        (struct syscall_sockaddr_in *)(uintptr_t)source_value;
    uint32_t *address_length =
        (uint32_t *)(uintptr_t)address_length_value;
    struct udp_socket *socket;
    uint8_t source_ip[NET_IPV4_ADDRESS_SIZE];
    uint16_t source_port;
    size_t received_length;

    if (!current_process || fd >= MAX_FD || !buffer_value || flags != 0u ||
        length > SIZE_MAX ||
        (source && (!address_length ||
                    *address_length < sizeof(struct syscall_sockaddr_in))) ||
        !(socket = descriptor_udp_socket(current_process->files[fd])) ||
        net_udp_socket_recvfrom(
            socket, (uint8_t *)(uintptr_t)buffer_value, (size_t)length,
            source_ip, &source_port, &received_length) != 0) {
        return UINT64_MAX;
    }
    if (source) {
        source->family = SYSCALL_AF_INET;
        net_store_be16((uint8_t *)&source->port, source_port);
        memory_copy(source->address, source_ip, NET_IPV4_ADDRESS_SIZE);
        memory_set(source->zero, 0u, sizeof(source->zero));
        *address_length = sizeof(*source);
    }
    return received_length;
}

static uint64_t syscall_socket_enosys(void)
{
    return UINT64_MAX;
}

static uint64_t syscall_icmp_echo(uint64_t address, uint64_t sequence)
{
    uint8_t destination[NET_IPV4_ADDRESS_SIZE];
    uint64_t elapsed_ticks;

    if (address == 0 || sequence > UINT16_MAX) {
        return UINT64_MAX;
    }
    memory_copy(destination, (const uint8_t *)(uintptr_t)address,
                sizeof(destination));
    if (net_icmp_echo(destination, (uint16_t)sequence, &elapsed_ticks) != 0) {
        return UINT64_MAX;
    }
    return elapsed_ticks;
}

static int signal_default_is_ignored(uint32_t signal)
{
    return signal == SIGNAL_SIGCHLD || signal == SIGNAL_SIGCONT ||
           signal == SIGNAL_SIGURG || signal == SIGNAL_SIGWINCH;
}

static uint64_t syscall_rt_sigaction(uint64_t signal, uint64_t action_address,
                                     uint64_t old_action_address, uint64_t sigset_size)
{
    struct syscall_sigaction *action = (struct syscall_sigaction *)(uintptr_t)action_address;
    struct syscall_sigaction *old_action = (struct syscall_sigaction *)(uintptr_t)old_action_address;
    struct task_signal_action *installed;

    if (!current_process || signal == 0 || signal >= TASK_SIGNAL_COUNT ||
        sigset_size != sizeof(uint64_t)) {
        return UINT64_MAX;
    }

    installed = &current_process->signal_actions[signal];
    if (old_action) {
        old_action->handler = installed->handler;
        old_action->mask = installed->mask;
        old_action->flags = installed->flags;
        old_action->restorer = installed->restorer;
    }

    if (action) {
        if (signal == SIGNAL_SIGKILL || signal == SIGNAL_SIGSTOP) {
            return UINT64_MAX;
        }
        if (action->handler != SIGNAL_HANDLER_DEFAULT &&
            action->handler != SIGNAL_HANDLER_IGNORE &&
            (((action->flags & SIGNAL_FLAG_RESTORER) == 0) || action->restorer == 0)) {
            return UINT64_MAX;
        }

        installed->handler = action->handler;
        installed->mask = sanitise_signal_mask(action->mask);
        installed->flags = action->flags;
        installed->restorer = action->restorer;
        if (action->handler == SIGNAL_HANDLER_IGNORE ||
            (action->handler == SIGNAL_HANDLER_DEFAULT && signal_default_is_ignored((uint32_t)signal))) {
            current_process->signal_pending &= ~signal_bit((uint32_t)signal);
        }
    }

    return 0;
}

static uint64_t syscall_rt_sigprocmask(uint64_t how, uint64_t set_address,
                                       uint64_t old_set_address, uint64_t sigset_size)
{
    const uint64_t *set = (const uint64_t *)(uintptr_t)set_address;
    uint64_t *old_set = (uint64_t *)(uintptr_t)old_set_address;
    uint64_t old_mask;

    if (!current_process || sigset_size != sizeof(uint64_t)) {
        return UINT64_MAX;
    }

    old_mask = current_process->signal_mask;
    if (old_set) {
        *old_set = old_mask;
    }
    if (!set) {
        return 0;
    }

    switch (how) {
    case SIGNAL_BLOCK:
        current_process->signal_mask |= *set;
        break;
    case SIGNAL_UNBLOCK:
        current_process->signal_mask &= ~*set;
        break;
    case SIGNAL_SETMASK:
        current_process->signal_mask = *set;
        break;
    default:
        return UINT64_MAX;
    }
    current_process->signal_mask = sanitise_signal_mask(current_process->signal_mask);
    return 0;
}

static uint64_t syscall_sigpending(uint64_t set_address, uint64_t sigset_size)
{
    uint64_t *set = (uint64_t *)(uintptr_t)set_address;

    if (!current_process || !set || sigset_size != sizeof(uint64_t)) {
        return UINT64_MAX;
    }
    *set = current_process->signal_pending;
    return 0;
}

static uint64_t syscall_kill(int64_t pid, uint64_t signal)
{
    struct process *target;
    struct task_signal_action *action;

    if (signal >= TASK_SIGNAL_COUNT || pid < 0) {
        return UINT64_MAX;
    }

    target = pid == 0 ? current_process : find_process_by_pid((uint64_t)pid);
    if (!target || target->exited) {
        return UINT64_MAX;
    }
    if (signal == 0) {
        return 0;
    }

    action = &target->signal_actions[signal];
    if (signal != SIGNAL_SIGKILL && signal != SIGNAL_SIGSTOP &&
        (action->handler == SIGNAL_HANDLER_IGNORE ||
         (action->handler == SIGNAL_HANDLER_DEFAULT && signal_default_is_ignored((uint32_t)signal)))) {
        return 0;
    }

    target->signal_pending |= signal_bit((uint32_t)signal);
    return 0;
}

static uint64_t syscall_rt_sigreturn(struct user_context *ctx)
{
    uint32_t depth;
    uintptr_t expected_stack;

    if (!current_process || current_process->signal_depth == 0) {
        return UINT64_MAX;
    }

    depth = current_process->signal_depth - 1;
    expected_stack = VMM_SIGNAL_STACK_TOP -
                     ((uintptr_t)(depth + 1) * PAGE_SIZE);
    if (ctx->rsp != expected_stack) {
        return UINT64_MAX;
    }

    current_process->signal_mask = sanitise_signal_mask(current_process->signal_saved_masks[depth]);
    *ctx = current_process->signal_contexts[depth];
    current_process->signal_depth = depth;
    return ctx->rax;
}

/* Returns the default-action signal number when the process must terminate. */
static uint32_t signal_prepare_user_return(struct user_context *ctx)
{
    uint64_t deliverable;

    if (!current_process) {
        return 0;
    }

    for (;;) {
        uint32_t signal;
        struct task_signal_action *action;
        uintptr_t return_slot;
        uint32_t depth;

        deliverable = current_process->signal_pending & ~current_process->signal_mask;
        if (current_process->signal_pending & signal_bit(SIGNAL_SIGKILL)) {
            deliverable |= signal_bit(SIGNAL_SIGKILL);
        }
        if (deliverable == 0) {
            return 0;
        }

        for (signal = 1; signal < TASK_SIGNAL_COUNT; signal++) {
            if ((deliverable & signal_bit(signal)) != 0) {
                break;
            }
        }
        current_process->signal_pending &= ~signal_bit(signal);
        action = &current_process->signal_actions[signal];

        if (signal != SIGNAL_SIGKILL && signal != SIGNAL_SIGSTOP &&
            (action->handler == SIGNAL_HANDLER_IGNORE ||
             (action->handler == SIGNAL_HANDLER_DEFAULT && signal_default_is_ignored(signal)))) {
            continue;
        }
        if (action->handler == SIGNAL_HANDLER_DEFAULT || signal == SIGNAL_SIGKILL ||
            signal == SIGNAL_SIGSTOP) {
            return signal;
        }
        if (current_process->signal_depth >= TASK_SIGNAL_MAX_DEPTH || action->restorer == 0) {
            return SIGNAL_SIGSEGV;
        }

        depth = current_process->signal_depth;
        return_slot = VMM_SIGNAL_STACK_TOP -
                      ((uintptr_t)(depth + 1) * PAGE_SIZE) - sizeof(uintptr_t);
        current_process->signal_contexts[depth] = *ctx;
        current_process->signal_saved_masks[depth] = current_process->signal_mask;
        *(uintptr_t *)return_slot = action->restorer;

        current_process->signal_mask |= action->mask;
        if ((action->flags & SIGNAL_FLAG_NODEFER) == 0) {
            current_process->signal_mask |= signal_bit(signal);
        }
        current_process->signal_mask = sanitise_signal_mask(current_process->signal_mask);
        current_process->signal_depth = depth + 1;

        ctx->rip = action->handler;
        ctx->rsp = return_slot;
        ctx->rdi = signal;
        ctx->rsi = 0;
        ctx->rdx = 0;

        if ((action->flags & SIGNAL_FLAG_RESETHAND) != 0) {
            action->handler = SIGNAL_HANDLER_DEFAULT;
            action->mask = 0;
            action->flags = 0;
            action->restorer = 0;
        }
        return 0;
    }
}

static int memory_protection_flags(uint64_t protection, uint32_t *out_flags)
{
    uint32_t flags = VMM_USER_FLAG_USER;

    if (!out_flags || (protection & ~(uint64_t)(SYSCALL_PROT_READ |
                                                SYSCALL_PROT_WRITE |
                                                SYSCALL_PROT_EXEC)) != 0 ||
        ((protection & SYSCALL_PROT_WRITE) != 0 &&
         (protection & SYSCALL_PROT_EXEC) != 0)) {
        return -1;
    }
    if ((protection & SYSCALL_PROT_READ) != 0) {
        flags |= VMM_USER_FLAG_READABLE;
    }
    if ((protection & SYSCALL_PROT_WRITE) != 0) {
        flags |= VMM_USER_FLAG_WRITABLE;
    }
    if ((protection & SYSCALL_PROT_EXEC) != 0) {
        flags |= VMM_USER_FLAG_EXECUTABLE;
    }
    *out_flags = flags;
    return 0;
}

static int page_round_length(uint64_t length, uintptr_t *out_length)
{
    if (!out_length || length == 0 || length > UINTPTR_MAX - (PAGE_SIZE - 1u)) {
        return -1;
    }
    *out_length = ((uintptr_t)length + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
    return *out_length == 0 ? -1 : 0;
}

static uint64_t syscall_brk(uint64_t requested_break)
{
    return vmm_resize_heap(&current_process->address_space,
                           (uintptr_t)requested_break);
}

static uint64_t syscall_mmap(uint64_t address, uint64_t length, uint64_t protection,
                             uint64_t flags, uint64_t fd, uint64_t offset)
{
    uintptr_t rounded_length;
    uintptr_t mapping_address;
    uint32_t vmm_flags;

    if (flags != (SYSCALL_MAP_PRIVATE | SYSCALL_MAP_ANONYMOUS) ||
        fd != UINT64_MAX || offset != 0 ||
        page_round_length(length, &rounded_length) != 0 ||
        memory_protection_flags(protection, &vmm_flags) != 0 ||
        rounded_length > VMM_MMAP_END - VMM_MMAP_BASE) {
        return UINT64_MAX;
    }
    if (address == 0) {
        mapping_address = vmm_find_free_range(&current_process->address_space,
                                              VMM_MMAP_BASE, VMM_MMAP_END,
                                              rounded_length);
    } else {
        mapping_address = (uintptr_t)address;
        if ((mapping_address & (PAGE_SIZE - 1u)) != 0 ||
            mapping_address < VMM_MMAP_BASE || mapping_address >= VMM_MMAP_END ||
            rounded_length > VMM_MMAP_END - mapping_address) {
            return UINT64_MAX;
        }
    }
    if (!mapping_address ||
        vmm_map_anonymous_region(&current_process->address_space, mapping_address,
                                 rounded_length, vmm_flags,
                                 VM_REGION_MMAP) != 0) {
        return UINT64_MAX;
    }
    return mapping_address;
}

static uint64_t syscall_munmap(uint64_t address, uint64_t length)
{
    uintptr_t rounded_length;

    if ((address & (PAGE_SIZE - 1u)) != 0 ||
        page_round_length(length, &rounded_length) != 0 ||
        vmm_unmap_region(&current_process->address_space, (uintptr_t)address,
                         rounded_length, VM_REGION_MMAP) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

static uint64_t syscall_mprotect(uint64_t address, uint64_t length,
                                 uint64_t protection)
{
    uintptr_t rounded_length;
    uint32_t vmm_flags;

    if ((address & (PAGE_SIZE - 1u)) != 0 ||
        page_round_length(length, &rounded_length) != 0 ||
        memory_protection_flags(protection, &vmm_flags) != 0 ||
        vmm_protect_region(&current_process->address_space, (uintptr_t)address,
                           rounded_length, vmm_flags,
                           VM_REGION_MMAP) != 0) {
        return UINT64_MAX;
    }
    return 0;
}

/* Fork: parent returns immediately and the child is placed on the run queue. */
static void syscall_fork(struct user_context *ctx)
{
    struct address_space child_space;
    struct process *child;
    struct user_context child_ctx;

    if (vmm_clone_user_address_space(&current_process->address_space,
                                     &child_space) != 0) {
        ctx->rax = UINT64_MAX;
        return;
    }

    child = process_create_user(&child_space, current_process->pid);
    if (child == 0) {
        vmm_destroy_user_address_space(&child_space);
        ctx->rax = UINT64_MAX;
        return;
    }

    for (int fd = 0; fd < MAX_FD; fd++) {
        child->files[fd] = current_process->files[fd];
        if (child->files[fd]) {
            descriptor_retain(child->files[fd]);
        }
    }
    process_signal_fork(child, current_process);
    child->tty_lflag = current_process->tty_lflag;
    copy_string(child->cwd, current_process->cwd, sizeof(child->cwd));

    /* Copy parent context to child, set child RAX = 0 */
    child_ctx = *ctx;
    child_ctx.rax = 0;

    /* Parent returns child PID */
    ctx->rax = child->pid;

    child->saved_ctx = child_ctx;
    child->has_saved_context = 1;
    if (scheduler_enqueue_process(child) != 0) {
        process_exit(child, -1);
        process_reap(child);
        ctx->rax = UINT64_MAX;
    }
}

/* Execve: load ELF and switch to new program */
static int exec_vfs_read_at(void *context, uint64_t offset, void *buffer,
                            size_t size)
{
    struct vfs_file *file = context;

    return vfs_read_at(file, buffer, size, offset) == size ? 0 : -1;
}

static void syscall_execve(struct user_context *ctx, uint64_t path_address, uint64_t argv_address, uint64_t envp_address)
{
    char path[VFS_MAX_PATH];
    struct vfs_file *file;
    struct elf_reader reader;
    struct loaded_program program;
    struct address_space new_space;
    struct exec_stack stack;

    if (resolve_process_path((const char *)path_address, path) != 0 ||
        vfs_open(path, VFS_O_RDONLY, 0u, &file) != 0) {
        ctx->rax = UINT64_MAX;
        return;
    }
    if (!file->inode || file->inode->type != VFS_FILE_REGULAR ||
        file->inode->size == 0u) {
        vfs_close(file);
        ctx->rax = UINT64_MAX;
        return;
    }

    reader.context = file;
    reader.size = file->inode->size;
    reader.read_at = exec_vfs_read_at;

    if (vmm_create_user_address_space(&new_space) != 0) {
        vfs_close(file);
        ctx->rax = UINT64_MAX;
        return;
    }

    if (elf_load_reader(&reader, &new_space, &program) != 0) {
        vfs_close(file);
        vmm_destroy_user_address_space(&new_space);
        ctx->rax = UINT64_MAX;
        return;
    }
    vfs_close(file);

    if (vmm_map_user_stack(&new_space) != 0) {
        vmm_destroy_user_address_space(&new_space);
        ctx->rax = UINT64_MAX;
        return;
    }

    if (map_user_signal_stacks(&new_space) != 0) {
        vmm_destroy_user_address_space(&new_space);
        ctx->rax = UINT64_MAX;
        return;
    }

    if (build_exec_stack(&new_space, path, argv_address, envp_address,
                         &stack) != 0) {
        vmm_destroy_user_address_space(&new_space);
        ctx->rax = UINT64_MAX;
        return;
    }

    exec_commit.retired_space = current_process->address_space;
    exec_commit.entry = program.entry;
    exec_commit.stack_top = stack.stack_top;
    exec_commit.pml4 = new_space.pml4_physical;
    exec_commit.argc = stack.argc;
    exec_commit.argv = stack.argv;
    exec_commit.envp = stack.envp;
    current_process->address_space = new_space;
    process_signal_exec(current_process);

    x86_64_switch_exec(new_space.pml4_physical);
}

__attribute__((noreturn)) void syscall_exec_after_switch(void)
{
    vmm_destroy_user_address_space(&exec_commit.retired_space);
    x86_64_enter_user(exec_commit.entry, exec_commit.stack_top,
                      exec_commit.pml4, exec_commit.argc,
                      exec_commit.argv, exec_commit.envp);
}

static void syscall_waitpid(struct user_context *ctx, uint64_t pid, uint64_t status_address, uint64_t options)
{
    int *status = (int *)status_address;
    struct process *child = 0;
    int64_t requested_pid = (int64_t)pid;
    int has_child = 0;
    int exit_code;

    if ((options & ~SYSCALL_WNOHANG) != 0u || requested_pid == 0 ||
        requested_pid < -1) {
        ctx->rax = UINT64_MAX;
        return;
    }

    for (size_t index = 0; index < process_table_capacity(); index++) {
        struct process *candidate = process_table_at(index);

        if (!candidate || candidate->pid == 0u ||
            candidate->ppid != current_process->pid ||
            (requested_pid > 0 && candidate->pid != (uint64_t)requested_pid)) {
            continue;
        }
        has_child = 1;
        if (candidate->exited) {
            child = candidate;
            break;
        }
        if (requested_pid > 0) {
            child = candidate;
        }
    }

    if (!has_child) {
        ctx->rax = UINT64_MAX;
        return;
    }

    if (!child || !child->exited) {
        if ((options & SYSCALL_WNOHANG) != 0u) {
            ctx->rax = 0;
            return;
        }
        if (scheduler_block_current(ctx, PROCESS_WAIT_CHILD,
                                    requested_pid == -1 ? UINT64_MAX : pid, 0,
                                    SYS_WAIT4) != 0) {
            ctx->rax = UINT64_MAX;
        }
        return;
    }

    exit_code = child->exit_code;
    if (status) {
        *status = exit_code;
    }

    ctx->rax = child->pid;
    process_reap(child);
}

__attribute__((noreturn)) static void syscall_exit(struct user_context *ctx, uint64_t code)
{
    (void)ctx;

    process_exit(current_process, (int)code);
    scheduler_wake_child_waiters(current_process->pid);
    scheduler_exit_current();
}

void syscall_init(void)
{
    uint64_t star = ((uint64_t)USER_SYSCALL_STAR_SELECTOR << 48) | ((uint64_t)KERNEL_CODE_SELECTOR << 32);

    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | EFER_SCE);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)x86_64_syscall_entry);
    wrmsr(MSR_SFMASK, RFLAGS_INTERRUPT_ENABLE);
    log_info("syscall: x86_64 syscall entry initialized");
}

/* syscall_dispatch: called from assembly with pointer to saved user context */
void syscall_dispatch(struct user_context *ctx)
{
    uint64_t number = ctx->rax;
    uint64_t arg0 = ctx->rdi;
    uint64_t arg1 = ctx->rsi;
    uint64_t arg2 = ctx->rdx;
    uint64_t arg3 = ctx->r10;
    uint64_t arg4 = ctx->r8;
    uint64_t arg5 = ctx->r9;
    uint32_t terminating_signal;

    switch (number) {
    case SYS_EXIT:
        syscall_exit(ctx, arg0);
    case SYS_WRITE:
        ctx->rax = syscall_write(ctx, arg0, arg1, arg2);
        break;
    case SYS_READ:
        ctx->rax = syscall_read(ctx, arg0, arg1, arg2);
        break;
    case SYS_OPEN:
        ctx->rax = syscall_open(arg0, arg1, arg2);
        break;
    case SYS_CLOSE:
        ctx->rax = syscall_close(arg0);
        break;
    case SYS_LSEEK:
        ctx->rax = syscall_lseek(arg0, arg1, arg2);
        break;
    case SYS_MMAP:
        ctx->rax = syscall_mmap(arg0, arg1, arg2, arg3, arg4, arg5);
        break;
    case SYS_MPROTECT:
        ctx->rax = syscall_mprotect(arg0, arg1, arg2);
        break;
    case SYS_MUNMAP:
        ctx->rax = syscall_munmap(arg0, arg1);
        break;
    case SYS_BRK:
        ctx->rax = syscall_brk(arg0);
        break;
    case SYS_RT_SIGACTION:
        ctx->rax = syscall_rt_sigaction(arg0, arg1, arg2, arg3);
        break;
    case SYS_RT_SIGPROCMASK:
        ctx->rax = syscall_rt_sigprocmask(arg0, arg1, arg2, arg3);
        break;
    case SYS_RT_SIGRETURN:
        ctx->rax = syscall_rt_sigreturn(ctx);
        break;
    case SYS_IOCTL:
        ctx->rax = syscall_ioctl(arg0, arg1, arg2);
        break;
    case SYS_STAT:
        ctx->rax = syscall_stat(arg0, arg1);
        break;
    case SYS_FSTAT:
        ctx->rax = syscall_fstat(arg0, arg1);
        break;
    case SYS_MKDIR:
        ctx->rax = syscall_mkdir(arg0, arg1);
        break;
    case SYS_UNLINK:
        ctx->rax = syscall_unlink(arg0);
        break;
    case SYS_RMDIR:
        ctx->rax = syscall_rmdir(arg0);
        break;
    case SYS_GETDENTS64:
        ctx->rax = syscall_getdents64(arg0, arg1, arg2);
        break;
    case SYS_GETPID:
        ctx->rax = syscall_getpid();
        break;
    case SYS_DUP2:
        ctx->rax = syscall_dup2(arg0, arg1);
        break;
    case SYS_DUP:
        ctx->rax = syscall_dup(arg0);
        break;
    case SYS_FORK:
        syscall_fork(ctx);
        break;
    case SYS_EXECVE:
        syscall_execve(ctx, arg0, arg1, arg2);
        break; /* never reached */
    case SYS_WAIT4:
        syscall_waitpid(ctx, arg0, arg1, arg2);
        break;
    case SYS_KILL:
        ctx->rax = syscall_kill((int64_t)arg0, arg1);
        break;
    case SYS_PIPE:
        ctx->rax = syscall_pipe(arg0);
        break;
    case SYS_UNAME:
        syscall_uname(arg0);
        ctx->rax = 0;
        break;
    case SYS_FSYNC:
        ctx->rax = syscall_fsync(arg0);
        break;
    case SYS_TRUNCATE:
        ctx->rax = syscall_truncate(arg0, arg1);
        break;
    case SYS_FTRUNCATE:
        ctx->rax = syscall_ftruncate(arg0, arg1);
        break;
    case SYS_RENAME:
        ctx->rax = syscall_rename(arg0, arg1);
        break;
    case SYS_CHDIR:
        ctx->rax = syscall_chdir(arg0);
        break;
    case SYS_READLINKAT:
        ctx->rax = syscall_readlinkat(arg0, arg1, arg2, arg3);
        break;
    case SYS_TIME:
        ctx->rax = timer_ticks();
        break;
    case SYS_RANDOM:
        ctx->rax = syscall_random();
        break;
    case SYS_READDIR:
        ctx->rax = syscall_readdir(arg0, arg1, arg2);
        break;
    case SYS_FSINFO:
        ctx->rax = syscall_fsinfo(arg0, arg1);
        break;
    case SYS_GETCWD:
        ctx->rax = syscall_getcwd(arg0, arg1);
        break;
    case SYS_ICMP_ECHO:
        ctx->rax = syscall_icmp_echo(arg0, arg1);
        break;
    case SYS_RT_SIGPENDING:
        ctx->rax = syscall_sigpending(arg0, arg1);
        break;
    case SYS_SOCKET:
        ctx->rax = syscall_socket(arg0, arg1, arg2);
        break;
    case SYS_BIND:
        ctx->rax = syscall_bind(arg0, arg1, arg2);
        break;
    case SYS_SENDTO:
        ctx->rax = syscall_sendto(arg0, arg1, arg2, arg3, arg4, arg5);
        break;
    case SYS_RECVFROM:
        ctx->rax = syscall_recvfrom(arg0, arg1, arg2, arg3, arg4, arg5);
        break;
    case SYS_CONNECT:
        ctx->rax = syscall_connect(arg0, arg1, arg2);
        break;
    case SYS_LISTEN:
    case SYS_ACCEPT:
        ctx->rax = syscall_socket_enosys();
        break;
    default:
        ctx->rax = UINT64_MAX;
        break;
    }

    terminating_signal = signal_prepare_user_return(ctx);
    if (terminating_signal != 0) {
        syscall_exit(ctx, terminating_signal);
    }
}

void syscall_run_selftest(void)
{
    const char *message = "[syscall-test] write dispatcher ok\r\n";
    uint64_t message_length = 36;
    struct user_context test_ctx = {0};

    test_ctx.rax = SYS_WRITE;
    test_ctx.rdi = 1;
    test_ctx.rsi = (uint64_t)message;
    test_ctx.rdx = message_length;
    syscall_dispatch(&test_ctx);

    if (test_ctx.rax == message_length) {
        log_info("syscall: dispatcher self-test passed");
    } else {
        log_error("syscall: dispatcher self-test failed");
    }
}

static uint64_t selftest_dispatch(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    struct user_context ctx = {0};

    ctx.rax = number;
    ctx.rdi = arg0;
    ctx.rsi = arg1;
    ctx.rdx = arg2;
    syscall_dispatch(&ctx);
    return ctx.rax;
}

void syscall_run_vfs_selftest(void)
{
    const char *dir_path = "/syscall-test";
    const char *file_path = "/syscall-test/file";
    char buffer[6];
    uint64_t fd;
    uint64_t append_fd;
    uint64_t grow_fd;

    if (selftest_dispatch(SYS_MKDIR, (uint64_t)dir_path, 0755, 0) != 0 ||
        selftest_dispatch(SYS_MKDIR, (uint64_t)dir_path, 0755, 0) != UINT64_MAX) {
        log_error("syscall: VFS self-test mkdir failed");
        return;
    }

    fd = selftest_dispatch(SYS_OPEN, (uint64_t)file_path, VFS_O_CREAT | VFS_O_TRUNC | 2u, 0644);
    if (fd == UINT64_MAX || selftest_dispatch(SYS_WRITE, fd, (uint64_t)"base", 4) != 4 ||
        selftest_dispatch(SYS_CLOSE, fd, 0, 0) != 0) {
        log_error("syscall: VFS self-test create/write failed");
        return;
    }

    append_fd = selftest_dispatch(SYS_OPEN, (uint64_t)file_path, VFS_O_APPEND | 2u, 0);
    grow_fd = selftest_dispatch(SYS_OPEN, (uint64_t)file_path, 2u, 0);
    if (append_fd == UINT64_MAX || grow_fd == UINT64_MAX ||
        selftest_dispatch(SYS_LSEEK, grow_fd, 0, 2) != 4 ||
        selftest_dispatch(SYS_WRITE, grow_fd, (uint64_t)"X", 1) != 1 ||
        selftest_dispatch(SYS_LSEEK, append_fd, 0, 0) != 0 ||
        selftest_dispatch(SYS_WRITE, append_fd, (uint64_t)"A", 1) != 1) {
        log_error("syscall: VFS self-test append failed");
        return;
    }
    selftest_dispatch(SYS_CLOSE, grow_fd, 0, 0);
    selftest_dispatch(SYS_CLOSE, append_fd, 0, 0);

    fd = selftest_dispatch(SYS_OPEN, (uint64_t)file_path, 2u, 0);
    if (fd == UINT64_MAX || selftest_dispatch(SYS_READ, fd, (uint64_t)buffer, sizeof(buffer)) != sizeof(buffer) ||
        buffer[0] != 'b' || buffer[1] != 'a' || buffer[2] != 's' ||
        buffer[3] != 'e' || buffer[4] != 'X' || buffer[5] != 'A') {
        log_error("syscall: VFS self-test append content mismatch");
        return;
    }
    selftest_dispatch(SYS_CLOSE, fd, 0, 0);

    fd = selftest_dispatch(SYS_OPEN, (uint64_t)file_path, VFS_O_TRUNC | 2u, 0);
    if (fd == UINT64_MAX || selftest_dispatch(SYS_READ, fd, (uint64_t)buffer, sizeof(buffer)) != 0 ||
        selftest_dispatch(SYS_WRITE, fd, (uint64_t)"Z", 1) != 1) {
        log_error("syscall: VFS self-test truncate failed");
        return;
    }

    if (selftest_dispatch(SYS_UNLINK, (uint64_t)file_path, 0, 0) != 0 ||
        selftest_dispatch(SYS_OPEN, (uint64_t)file_path, 2u, 0) != UINT64_MAX ||
        selftest_dispatch(SYS_WRITE, fd, (uint64_t)"Q", 1) != 1 ||
        selftest_dispatch(SYS_CLOSE, fd, 0, 0) != 0 ||
        selftest_dispatch(SYS_UNLINK, (uint64_t)dir_path, 0, 0) != UINT64_MAX ||
        selftest_dispatch(SYS_RMDIR, (uint64_t)dir_path, 0, 0) != 0 ||
        selftest_dispatch(SYS_RMDIR, (uint64_t)dir_path, 0, 0) != UINT64_MAX) {
        log_error("syscall: VFS self-test unlink/rmdir failed");
        return;
    }

    log_info("syscall: VFS truncate/append/mkdir/unlink/rmdir self-test passed");
}
