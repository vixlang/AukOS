#include "include/aukos/scheduler.h"

#include "include/aukos/log.h"
#include "include/aukos/pipe.h"

#include <stddef.h>

static struct thread *run_queue[SCHEDULER_RUN_QUEUE_CAPACITY];
static size_t run_queue_count;
static struct process *user_run_queue[SCHEDULER_RUN_QUEUE_CAPACITY];
static size_t user_run_queue_count;
static struct thread *current_thread;
static volatile uint64_t scheduler_tick_count;

extern void x86_64_switch_and_resume(uintptr_t pml4,
                                     struct user_context *ctx)
    __attribute__((noreturn));
extern void x86_64_switch_exit(uintptr_t pml4, struct process *exited)
    __attribute__((noreturn));
extern void x86_64_resume_user(struct user_context *ctx)
    __attribute__((noreturn));

static struct process *exit_resume_process;

static void scheduler_test_thread(void *context)
{
    uint64_t *counter = context;

    (*counter)++;
}

void scheduler_init(void)
{
    run_queue_count = 0;
    user_run_queue_count = 0;
    current_thread = 0;
    scheduler_tick_count = 0;
    log_info("sched: single-core run queue initialized");
}

int scheduler_enqueue(struct thread *thread)
{
    if (thread == 0 || run_queue_count == SCHEDULER_RUN_QUEUE_CAPACITY) {
        return -1;
    }

    thread->state = TASK_READY;
    run_queue[run_queue_count] = thread;
    run_queue_count++;
    return 0;
}

static int process_is_queued(struct process *process)
{
    for (size_t index = 0; index < user_run_queue_count; index++) {
        if (user_run_queue[index] == process) {
            return 1;
        }
    }
    return 0;
}

int scheduler_enqueue_process(struct process *process)
{
    if (!process || process->pid == 0u || process_is_queued(process) ||
        user_run_queue_count == SCHEDULER_RUN_QUEUE_CAPACITY) {
        return -1;
    }
    process->state = PROCESS_RUNNABLE;
    user_run_queue[user_run_queue_count++] = process;
    return 0;
}

void scheduler_remove_process(struct process *process)
{
    for (size_t index = 0; index < user_run_queue_count;) {
        if (user_run_queue[index] == process) {
            for (size_t move = index + 1; move < user_run_queue_count; move++) {
                user_run_queue[move - 1] = user_run_queue[move];
            }
            user_run_queue[--user_run_queue_count] = 0;
            continue;
        }
        index++;
    }
}

static struct process *scheduler_take_next_process(void)
{
    while (user_run_queue_count != 0u) {
        struct process *process = user_run_queue[0];

        for (size_t index = 1; index < user_run_queue_count; index++) {
            user_run_queue[index - 1] = user_run_queue[index];
        }
        user_run_queue[--user_run_queue_count] = 0;
        if (process && process->state == PROCESS_RUNNABLE) {
            return process;
        }
    }
    return 0;
}

__attribute__((noreturn)) static void scheduler_switch_to(struct process *next)
{
    current_process = next;
    next->state = PROCESS_RUNNING;
    next->wait_reason = PROCESS_WAIT_NONE;
    next->wait_pid = 0;
    next->wait_pipe = 0;
    x86_64_switch_and_resume(next->address_space.pml4_physical,
                             &next->saved_ctx);
}

int scheduler_block_current(struct user_context *ctx,
                            enum process_wait_reason reason,
                            uint64_t wait_pid, struct pipe_object *pipe,
                            uint64_t syscall_number)
{
    struct process *blocked = current_process;
    struct process *next;

    if (!blocked || !ctx || reason == PROCESS_WAIT_NONE) {
        return -1;
    }
    next = scheduler_take_next_process();
    if (!next) {
        return -1;
    }
    blocked->saved_ctx = *ctx;
    blocked->saved_ctx.rip -= 2u;
    blocked->saved_ctx.rax = syscall_number;
    blocked->has_saved_context = 1;
    blocked->state = PROCESS_BLOCKED;
    blocked->wait_reason = reason;
    blocked->wait_pid = wait_pid;
    blocked->wait_pipe = pipe;
    scheduler_switch_to(next);
}

static void scheduler_wake(struct process *process)
{
    if (!process || process->state != PROCESS_BLOCKED) {
        return;
    }
    (void)scheduler_enqueue_process(process);
}

void scheduler_wake_child_waiters(uint64_t child_pid)
{
    for (size_t index = 0; index < process_table_capacity(); index++) {
        struct process *process = process_table_at(index);

        if (process->pid != 0u && process->state == PROCESS_BLOCKED &&
            process->wait_reason == PROCESS_WAIT_CHILD &&
            (process->wait_pid == child_pid || process->wait_pid == UINT64_MAX)) {
            scheduler_wake(process);
        }
    }
}

void scheduler_pipe_changed(struct pipe_object *pipe)
{
    for (size_t index = 0; index < process_table_capacity(); index++) {
        struct process *process = process_table_at(index);
        int ready = 0;

        if (process->pid == 0u || process->state != PROCESS_BLOCKED ||
            process->wait_pipe != pipe) {
            continue;
        }
        if (process->wait_reason == PROCESS_WAIT_PIPE_READ) {
            ready = pipe_data_size(pipe) != 0u || pipe_writer_count(pipe) == 0u;
        } else if (process->wait_reason == PROCESS_WAIT_PIPE_WRITE) {
            ready = pipe_space_size(pipe) != 0u || pipe_reader_count(pipe) == 0u;
        }
        if (ready) {
            scheduler_wake(process);
        }
    }
}

void scheduler_exit_current(void)
{
    struct process *exited = current_process;
    struct process *next = scheduler_take_next_process();

    if (next) {
        current_process = next;
        next->state = PROCESS_RUNNING;
        next->wait_reason = PROCESS_WAIT_NONE;
        next->wait_pid = 0;
        next->wait_pipe = 0;
    }
    exit_resume_process = next;
    x86_64_switch_exit(next ? next->address_space.pml4_physical
                            : vmm_kernel_address_space()->pml4_physical,
                       exited);
}

void scheduler_exit_after_switch(struct process *exited)
{
    process_release_address_space(exited);
    if (exit_resume_process) {
        x86_64_resume_user(&exit_resume_process->saved_ctx);
    }
    log_info("sched: no runnable user process; halted");
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

size_t scheduler_runnable_count(void)
{
    return user_run_queue_count;
}

struct thread *scheduler_current_thread(void)
{
    return current_thread;
}

static void scheduler_run_once(void)
{
    struct thread *thread;

    if (run_queue_count == 0) {
        return;
    }

    thread = run_queue[0];
    for (size_t index = 1; index < run_queue_count; index++) {
        run_queue[index - 1] = run_queue[index];
    }
    run_queue_count--;

    current_thread = thread;
    thread->state = TASK_RUNNING;
    thread->entry(thread->context);
    thread->state = TASK_EXITED;
}

void scheduler_run_selftest(void)
{
    uint64_t counter = 0;
    struct thread *first = thread_create_kernel("sched-test-a", scheduler_test_thread, &counter);
    struct thread *second = thread_create_kernel("sched-test-b", scheduler_test_thread, &counter);

    if (first == 0 || second == 0) {
        log_error("sched: self-test failed to create threads");
        return;
    }

    if (scheduler_enqueue(first) != 0 || scheduler_enqueue(second) != 0) {
        log_error("sched: self-test failed to enqueue threads");
        return;
    }

    scheduler_run_once();
    scheduler_run_once();

    if (counter == 2 && first->state == TASK_EXITED && second->state == TASK_EXITED) {
        log_info("sched: cooperative self-test passed");
    } else {
        log_error("sched: cooperative self-test failed");
    }
}

void scheduler_on_tick(void)
{
    scheduler_tick_count++;
}

uint64_t scheduler_ticks(void)
{
    return scheduler_tick_count;
}
