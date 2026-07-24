#include <aukos/descriptor.h>
#include <aukos/log.h>
#include <aukos/memory.h>
#include <aukos/pipe.h>
#include <aukos/scheduler.h>
#include <aukos/task.h>
#include <aukos/vmm.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static struct descriptor fake_descriptors[8];
static size_t fake_descriptor_count;
static struct pipe_object fake_pipe;
static struct address_space kernel_space = {.pml4_physical = 0x1000u};
static int descriptor_releases;
static int pipe_notifications;
static int address_space_destroys;

void log_write(enum log_level level, const char *message)
{
    (void)level;
    (void)message;
}

void descriptor_init(void)
{
    fake_descriptor_count = 0u;
}

void pipe_init(void) {}

struct descriptor *descriptor_create_standard(uint32_t standard_fd)
{
    struct descriptor *descriptor = &fake_descriptors[fake_descriptor_count++];
    descriptor->type = DESCRIPTOR_STANDARD;
    descriptor->references = 1u;
    descriptor->object.standard_fd = standard_fd;
    return descriptor;
}

void descriptor_release(struct descriptor *descriptor)
{
    if (!descriptor || descriptor->type == DESCRIPTOR_NONE) return;
    descriptor_releases++;
    descriptor->references = 0u;
    descriptor->type = DESCRIPTOR_NONE;
}

struct pipe_object *descriptor_pipe(struct descriptor *descriptor,
                                    int *write_end)
{
    if (!descriptor || (descriptor->type != DESCRIPTOR_PIPE_READ &&
                        descriptor->type != DESCRIPTOR_PIPE_WRITE)) {
        return 0;
    }
    if (write_end) *write_end = descriptor->type == DESCRIPTOR_PIPE_WRITE;
    return descriptor->object.pipe;
}

void scheduler_remove_process(struct process *process)
{
    (void)process;
}

void scheduler_pipe_changed(struct pipe_object *pipe)
{
    assert(pipe == &fake_pipe);
    pipe_notifications++;
}

void vmm_destroy_user_address_space(struct address_space *space)
{
    if (space->pml4_physical != 0u &&
        space->pml4_physical != kernel_space.pml4_physical) {
        address_space_destroys++;
        space->pml4_physical = 0u;
    }
}

const struct address_space *vmm_kernel_address_space(void)
{
    return &kernel_space;
}

void *pmm_alloc_page(void)
{
    return 0;
}

int main(void)
{
    struct address_space space = {.pml4_physical = 0x2000u};
    struct process *parent;
    struct process *child;
    uint64_t child_pid;

    tasking_init();
    parent = current_process;
    assert(parent != 0 && parent->pid != 0u);
    descriptor_releases = 0;

    child = process_create_user(&space, parent->pid);
    assert(child != 0);
    child_pid = child->pid;
    fake_descriptors[3].type = DESCRIPTOR_PIPE_WRITE;
    fake_descriptors[3].references = 1u;
    fake_descriptors[3].object.pipe = &fake_pipe;
    child->files[7] = &fake_descriptors[3];

    process_exit(child, 23);
    assert(child->pid == child_pid);
    assert(child->ppid == parent->pid);
    assert(child->exit_code == 23 && child->exited);
    assert(child->state == PROCESS_EXITED);
    assert(child->files[7] == 0);
    assert(descriptor_releases == 1);
    assert(pipe_notifications == 1);
    assert(address_space_destroys == 0);

    process_reap(child);
    assert(child->pid == 0u);
    assert(address_space_destroys == 1);
    process_reap(child);
    assert(address_space_destroys == 1);

    for (int round = 0; round < 40; round++) {
        space.pml4_physical = 0x3000u + (uintptr_t)round * 0x1000u;
        child = process_create_user(&space, parent->pid);
        assert(child != 0);
        process_exit(child, round);
        assert(child->pid != 0u && child->exit_code == round);
        process_reap(child);
        assert(child->pid == 0u);
    }

    puts("task_lifecycle_test: PASS");
    return 0;
}
