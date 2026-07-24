#include <aukos/log.h>
#include <aukos/pipe.h>
#include <aukos/scheduler.h>

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

static struct process test_processes[4];
static jmp_buf switch_target;
struct process *current_process;
static int released_address_space;
static struct address_space test_kernel_space = {.pml4_physical = 0x1000u};

void log_write(enum log_level level, const char *message)
{
    (void)level;
    (void)message;
}

struct thread *thread_create_kernel(const char *name,
                                    void (*entry)(void *context),
                                    void *context)
{
    (void)name;
    (void)entry;
    (void)context;
    return NULL;
}

struct process *process_table_at(size_t index)
{
    return index < 4u ? &test_processes[index] : NULL;
}

size_t process_table_capacity(void)
{
    return 4u;
}

__attribute__((noreturn)) void x86_64_switch_and_resume(
    uintptr_t pml4, struct user_context *ctx)
{
    (void)pml4;
    (void)ctx;
    longjmp(switch_target, 1);
}

const struct address_space *vmm_kernel_address_space(void)
{
    return &test_kernel_space;
}

void process_release_address_space(struct process *process)
{
    (void)process;
    released_address_space++;
}

__attribute__((noreturn)) void x86_64_resume_user(struct user_context *ctx)
{
    (void)ctx;
    longjmp(switch_target, 2);
}

__attribute__((noreturn)) void x86_64_switch_exit(uintptr_t pml4,
                                                  struct process *exited)
{
    (void)pml4;
    scheduler_exit_after_switch(exited);
}

int main(void)
{
    struct user_context context = {0};
    struct pipe_object *pipe;
    uint8_t byte = 42u;
    size_t moved;
    static struct process capacity_processes[SCHEDULER_RUN_QUEUE_CAPACITY + 1u];

    scheduler_init();
    test_processes[0].pid = 1u;
    test_processes[0].state = PROCESS_RUNNING;
    test_processes[1].pid = 2u;
    test_processes[1].state = PROCESS_RUNNABLE;
    current_process = &test_processes[0];

    assert(scheduler_block_current(&context, PROCESS_WAIT_CHILD, 2u, NULL,
                                   61u) == -1);
    assert(scheduler_enqueue_process(&test_processes[1]) == 0);
    assert(scheduler_enqueue_process(&test_processes[1]) == -1);
    context.rip = 0x102u;
    if (setjmp(switch_target) == 0) {
        (void)scheduler_block_current(&context, PROCESS_WAIT_CHILD, 2u, NULL,
                                      61u);
        assert(0);
    }
    assert(current_process == &test_processes[1]);
    assert(test_processes[0].state == PROCESS_BLOCKED);
    assert(test_processes[0].saved_ctx.rip == 0x100u);
    assert(test_processes[0].saved_ctx.rax == 61u);
    assert(scheduler_runnable_count() == 0u);

    scheduler_wake_child_waiters(3u);
    assert(scheduler_runnable_count() == 0u);
    scheduler_wake_child_waiters(2u);
    assert(scheduler_runnable_count() == 1u);
    scheduler_wake_child_waiters(2u);
    assert(scheduler_runnable_count() == 1u);

    pipe_init();
    pipe = pipe_allocate();
    assert(pipe != NULL);
    pipe_endpoint_open(pipe, PIPE_READ_END);
    pipe_endpoint_open(pipe, PIPE_WRITE_END);
    test_processes[2].pid = 3u;
    test_processes[2].state = PROCESS_BLOCKED;
    test_processes[2].wait_reason = PROCESS_WAIT_PIPE_READ;
    test_processes[2].wait_pipe = pipe;
    scheduler_pipe_changed(pipe);
    assert(scheduler_runnable_count() == 1u);
    assert(pipe_write(pipe, &byte, 1u, &moved) == PIPE_IO_OK);
    scheduler_pipe_changed(pipe);
    assert(test_processes[2].state == PROCESS_RUNNABLE);
    assert(scheduler_runnable_count() == 2u);
    scheduler_pipe_changed(pipe);
    assert(scheduler_runnable_count() == 2u);

    scheduler_init();
    for (size_t index = 0; index < SCHEDULER_RUN_QUEUE_CAPACITY; index++) {
        capacity_processes[index].pid = index + 10u;
        capacity_processes[index].state = PROCESS_RUNNABLE;
        assert(scheduler_enqueue_process(&capacity_processes[index]) == 0);
    }
    capacity_processes[SCHEDULER_RUN_QUEUE_CAPACITY].pid = 100u;
    capacity_processes[SCHEDULER_RUN_QUEUE_CAPACITY].state = PROCESS_RUNNABLE;
    assert(scheduler_enqueue_process(
               &capacity_processes[SCHEDULER_RUN_QUEUE_CAPACITY]) == -1);
    assert(scheduler_runnable_count() == SCHEDULER_RUN_QUEUE_CAPACITY);

    scheduler_init();
    current_process = &test_processes[0];
    test_processes[0].state = PROCESS_EXITED;
    test_processes[1].state = PROCESS_RUNNABLE;
    test_processes[1].address_space.pml4_physical = 0x2000u;
    assert(scheduler_enqueue_process(&test_processes[1]) == 0);
    if (setjmp(switch_target) == 0) {
        scheduler_exit_current();
    }
    assert(current_process == &test_processes[1]);
    assert(released_address_space == 1);

    puts("scheduler_test: PASS");
    return 0;
}
