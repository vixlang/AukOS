#ifndef AUKOS_SCHEDULER_H
#define AUKOS_SCHEDULER_H

#include "task.h"

#define SCHEDULER_RUN_QUEUE_CAPACITY 32u

void scheduler_init(void);
int scheduler_enqueue(struct thread *thread);
struct thread *scheduler_current_thread(void);
void scheduler_run_selftest(void);
void scheduler_on_tick(void);
uint64_t scheduler_ticks(void);
int scheduler_enqueue_process(struct process *process);
void scheduler_remove_process(struct process *process);
int scheduler_block_current(struct user_context *ctx,
                            enum process_wait_reason reason,
                            uint64_t wait_pid, struct pipe_object *pipe,
                            uint64_t syscall_number);
void scheduler_wake_child_waiters(uint64_t child_pid);
void scheduler_pipe_changed(struct pipe_object *pipe);
void scheduler_exit_current(void) __attribute__((noreturn));
void scheduler_exit_after_switch(struct process *exited) __attribute__((noreturn));
size_t scheduler_runnable_count(void);

#endif
