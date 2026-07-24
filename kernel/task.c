#include "include/aukos/task.h"

#include "include/aukos/descriptor.h"
#include "include/aukos/log.h"
#include "include/aukos/memory.h"
#include "include/aukos/pipe.h"
#include "include/aukos/scheduler.h"

#include <stddef.h>
#include <stdint.h>

#define MAX_PROCESSES 16
#define MAX_THREADS 32
#define KERNEL_STACK_SIZE PAGE_SIZE
#define TTY_LFLAG_DEFAULT 0x000au

static struct process processes[MAX_PROCESSES];
static struct thread threads[MAX_THREADS];
static uint64_t next_pid = 1;
static uint64_t next_tid = 1;
static struct process *kernel_process;
struct process *current_process;

static void initialize_signals(struct process *process)
{
    for (size_t signal = 0; signal < TASK_SIGNAL_COUNT; signal++) {
        process->signal_actions[signal].handler = 0;
        process->signal_actions[signal].mask = 0;
        process->signal_actions[signal].flags = 0;
        process->signal_actions[signal].restorer = 0;
    }
    process->signal_mask = 0;
    process->signal_pending = 0;
    process->signal_depth = 0;
}

static void initialize_process_state(struct process *process)
{
    process->state = PROCESS_RUNNABLE;
    process->wait_reason = PROCESS_WAIT_NONE;
    process->wait_pid = 0;
    process->wait_pipe = 0;
    process->tty_lflag = TTY_LFLAG_DEFAULT;
    process->cwd[0] = '/';
    process->cwd[1] = '\0';
}

static void copy_name(char *destination, const char *source)
{
    size_t index = 0;

    while (index + 1 < TASK_NAME_LENGTH && source[index] != '\0') {
        destination[index] = source[index];
        index++;
    }

    destination[index] = '\0';
}

static int initialize_descriptors(struct process *process, int with_standard)
{
    for (int fd = 0; fd < MAX_FD; fd++) {
        process->files[fd] = 0;
    }
    if (!with_standard) {
        return 0;
    }
    for (uint32_t fd = 0; fd <= 2u; fd++) {
        process->files[fd] = descriptor_create_standard(fd);
        if (!process->files[fd]) {
            for (uint32_t release_fd = 0; release_fd < fd; release_fd++) {
                descriptor_release(process->files[release_fd]);
                process->files[release_fd] = 0;
            }
            return -1;
        }
    }
    return 0;
}

void tasking_init(void)
{
    descriptor_init();
    pipe_init();
    kernel_process = process_create_kernel();
    if (kernel_process == 0) {
        log_error("task: failed to create kernel process");
        return;
    }

    current_process = kernel_process;
    kernel_process->state = PROCESS_RUNNING;
    log_info("task: process/thread model initialized");
}

struct process *process_create_kernel(void)
{
    for (size_t index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = &processes[index];

        if (process->pid != 0) {
            continue;
        }

        process->pid = next_pid++;
        process->ppid = 0;
        process->address_space = *vmm_kernel_address_space();
        process->exit_code = 0;
        process->exited = 0;
        process->has_saved_context = 0;
        initialize_process_state(process);
        initialize_signals(process);
        if (initialize_descriptors(process, 1) != 0) {
            process->pid = 0;
            return 0;
        }
        return process;
    }

    return 0;
}

struct process *process_create_user(struct address_space *space, uint64_t ppid)
{
    for (size_t index = 0; index < MAX_PROCESSES; index++) {
        struct process *process = &processes[index];

        if (process->pid != 0) {
            continue;
        }

        process->pid = next_pid++;
        process->ppid = ppid;
        process->address_space = *space;
        process->exit_code = 0;
        process->exited = 0;
        process->has_saved_context = 0;
        initialize_process_state(process);
        initialize_signals(process);
        if (initialize_descriptors(process, ppid == 0) != 0) {
            process->pid = 0;
            return 0;
        }
        return process;
    }

    return 0;
}

struct thread *thread_create_kernel(const char *name, void (*entry)(void *context), void *context)
{
    for (size_t index = 0; index < MAX_THREADS; index++) {
        struct thread *thread = &threads[index];
        void *stack;

        if (thread->tid != 0) {
            continue;
        }

        stack = pmm_alloc_page();
        if (stack == 0) {
            return 0;
        }

        thread->tid = next_tid++;
        thread->state = TASK_READY;
        thread->process = kernel_process;
        thread->kernel_stack_top = (uintptr_t)stack + KERNEL_STACK_SIZE;
        thread->entry = entry;
        thread->context = context;
        copy_name(thread->name, name);
        return thread;
    }

    return 0;
}

struct process *find_process_by_pid(uint64_t pid)
{
    for (size_t index = 0; index < MAX_PROCESSES; index++) {
        if (processes[index].pid == pid) {
            return &processes[index];
        }
    }
    return 0;
}

struct process *process_table_at(size_t index)
{
    return index < MAX_PROCESSES ? &processes[index] : 0;
}

size_t process_table_capacity(void)
{
    return MAX_PROCESSES;
}

void process_reap(struct process *process)
{
    if (!process || !process->exited) {
        return;
    }

    scheduler_remove_process(process);
    for (int fd = 0; fd < MAX_FD; fd++) {
        struct pipe_object *pipe = descriptor_pipe(process->files[fd], 0);

        descriptor_release(process->files[fd]);
        process->files[fd] = 0;
        if (pipe) {
            scheduler_pipe_changed(pipe);
        }
    }
    process_release_address_space(process);
    process->pid = 0;
    process->ppid = 0;
    process->has_saved_context = 0;
    process->state = PROCESS_EXITED;
    process->wait_reason = PROCESS_WAIT_NONE;
    process->wait_pipe = 0;
}

void process_release_address_space(struct process *process)
{
    if (!process) {
        return;
    }
    vmm_destroy_user_address_space(&process->address_space);
}

void process_exit(struct process *process, int exit_code)
{
    if (!process || process->exited) {
        return;
    }

    scheduler_remove_process(process);
    process->exit_code = exit_code;
    process->exited = 1;
    process->state = PROCESS_EXITED;
    process->wait_reason = PROCESS_WAIT_NONE;
    process->wait_pid = 0;
    process->wait_pipe = 0;

    for (int fd = 0; fd < MAX_FD; fd++) {
        struct pipe_object *pipe = descriptor_pipe(process->files[fd], 0);

        descriptor_release(process->files[fd]);
        process->files[fd] = 0;
        if (pipe) {
            scheduler_pipe_changed(pipe);
        }
    }
}

void process_signal_fork(struct process *child, const struct process *parent)
{
    for (size_t signal = 0; signal < TASK_SIGNAL_COUNT; signal++) {
        child->signal_actions[signal] = parent->signal_actions[signal];
    }
    child->signal_mask = parent->signal_mask;
    child->signal_pending = 0;
    child->signal_depth = 0;
}

void process_signal_exec(struct process *process)
{
    for (size_t signal = 1; signal < TASK_SIGNAL_COUNT; signal++) {
        if (process->signal_actions[signal].handler != 1) {
            process->signal_actions[signal].handler = 0;
            process->signal_actions[signal].mask = 0;
            process->signal_actions[signal].flags = 0;
            process->signal_actions[signal].restorer = 0;
        }
    }
    process->signal_depth = 0;
}
