#ifndef AUKOS_TASK_H
#define AUKOS_TASK_H

#include "vmm.h"

#include <stddef.h>
#include <stdint.h>

#define TASK_NAME_LENGTH 32
#define MAX_FD 32
#define TASK_SIGNAL_COUNT 32
#define TASK_SIGNAL_MAX_DEPTH 8
#define TASK_CWD_MAX 256

struct descriptor;
struct pipe_object;

struct task_signal_action {
    uintptr_t handler;
    uint64_t mask;
    uint64_t flags;
    uintptr_t restorer;
};

enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_EXITED,
};

enum process_state {
    PROCESS_RUNNABLE = 0,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_EXITED,
};

enum process_wait_reason {
    PROCESS_WAIT_NONE = 0,
    PROCESS_WAIT_CHILD,
    PROCESS_WAIT_PIPE_READ,
    PROCESS_WAIT_PIPE_WRITE,
};

/* Must match the push order in syscall_entry.asm */
struct user_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct process {
    uint64_t pid;
    uint64_t ppid;
    struct address_space address_space;
    struct descriptor *files[MAX_FD];
    struct user_context saved_ctx;
    int exit_code;
    int exited;
    int has_saved_context;
    enum process_state state;
    enum process_wait_reason wait_reason;
    uint64_t wait_pid;
    struct pipe_object *wait_pipe;
    uint32_t tty_lflag;
    char cwd[TASK_CWD_MAX];
    struct task_signal_action signal_actions[TASK_SIGNAL_COUNT];
    uint64_t signal_mask;
    uint64_t signal_pending;
    uint32_t signal_depth;
    struct user_context signal_contexts[TASK_SIGNAL_MAX_DEPTH];
    uint64_t signal_saved_masks[TASK_SIGNAL_MAX_DEPTH];
};

struct thread {
    uint64_t tid;
    enum task_state state;
    struct process *process;
    uintptr_t kernel_stack_top;
    uintptr_t user_stack_top;
    uintptr_t entry_point;
    void (*entry)(void *context);
    void *context;
    char name[TASK_NAME_LENGTH];
};

void tasking_init(void);
struct process *process_create_kernel(void);
struct process *process_create_user(struct address_space *space, uint64_t ppid);
struct thread *thread_create_kernel(const char *name, void (*entry)(void *context), void *context);
struct process *find_process_by_pid(uint64_t pid);
struct process *process_table_at(size_t index);
size_t process_table_capacity(void);
void process_reap(struct process *process);
void process_release_address_space(struct process *process);
int process_fork(struct process *parent, struct user_context *ctx);
int process_execve(struct process *proc, const char *path, char *const argv[], char *const envp[]);
int process_waitpid(struct process *parent, int child_pid);
void process_exit(struct process *proc, int exit_code);
void process_signal_fork(struct process *child, const struct process *parent);
void process_signal_exec(struct process *proc);

extern struct process *current_process;

#endif
