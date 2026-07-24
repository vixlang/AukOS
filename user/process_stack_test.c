#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define EXPECTED_FAULT_STATUS 11
#define EXEC_STACK_GUARD_ADDRESS 0x000000007feff000ull

static int fail(const char *reason)
{
    printf("[process_stack_test] FAIL %s\n", reason);
    return 1;
}

__attribute__((noinline)) static uint64_t deep_stack(unsigned depth,
                                                     uint64_t seed)
{
    volatile unsigned char storage[3072];

    storage[0] = (unsigned char)seed;
    storage[sizeof(storage) - 1u] = (unsigned char)(seed >> 8);
    if (depth == 0u) {
        return (uint64_t)storage[0] + storage[sizeof(storage) - 1u];
    }
    return deep_stack(depth - 1u, seed + 0x101u) + storage[0] +
           storage[sizeof(storage) - 1u];
}

__attribute__((noinline)) static int child_stack_after_fork(void)
{
    volatile unsigned char child_stack[8192];

    child_stack[0] = 0x5au;
    child_stack[sizeof(child_stack) - 1u] = 0xa5u;
    return child_stack[0] == 0x5au &&
                   child_stack[sizeof(child_stack) - 1u] == 0xa5u
               ? 0
               : 2;
}

__attribute__((noinline)) static int fork_on_deep_stack(unsigned depth)
{
    volatile unsigned char storage[3072];

    storage[0] = (unsigned char)depth;
    storage[sizeof(storage) - 1u] = (unsigned char)(depth + 1u);
    if (depth != 0u) {
        return fork_on_deep_stack(depth - 1u) +
               (storage[0] == (unsigned char)depth ? 0 : 1000);
    }

    int status = -1;
    pid_t child = fork();
    if (child == 0) {
        _exit(child_stack_after_fork());
    }
    return child > 0 && waitpid(child, &status, 0) == child && status == 0
               ? 0
               : -1;
}

static int test_parent_waits_first(void)
{
    int status = -1;
    pid_t child = fork();

    if (child == 0) _exit(42);
    return child > 0 && waitpid(child, &status, 0) == child && status == 42
               ? 0
               : -1;
}

static int test_pipe_eof_then_wait(void)
{
    int descriptors[2];
    int status = -1;
    char byte = 0;
    pid_t child;

    if (pipe(descriptors) != 0) return -1;
    child = fork();
    if (child == 0) {
        close(descriptors[0]);
        if (write(descriptors[1], "x", 1u) != 1) _exit(2);
        close(descriptors[1]);
        _exit(7);
    }
    if (child < 0) return -1;
    close(descriptors[1]);
    if (read(descriptors[0], &byte, 1u) != 1 || byte != 'x' ||
        read(descriptors[0], &byte, 1u) != 0 || close(descriptors[0]) != 0 ||
        waitpid(child, &status, 0) != child || status != 7 ||
        waitpid(child, &status, 0) != -1) {
        return -1;
    }
    return 0;
}

static int test_nohang_and_wait_any(void)
{
    int descriptors[2];
    int status = -1;
    char byte = 0;
    pid_t blocked;
    pid_t second;
    pid_t waited;

    if (pipe(descriptors) != 0) return -1;
    blocked = fork();
    if (blocked == 0) {
        close(descriptors[1]);
        _exit(read(descriptors[0], &byte, 1u) == 1 ? 3 : 4);
    }
    if (blocked < 0 || waitpid(blocked, &status, WNOHANG) != 0) return -1;
    second = fork();
    if (second == 0) _exit(5);
    if (second < 0 || write(descriptors[1], "y", 1u) != 1) return -1;
    close(descriptors[0]);
    close(descriptors[1]);

    waited = waitpid(-1, &status, 0);
    if ((waited != blocked && waited != second) ||
        (waited == blocked && status != 3) ||
        (waited == second && status != 5)) {
        return -1;
    }
    waited = waitpid(-1, &status, 0);
    if ((waited != blocked && waited != second) ||
        (waited == blocked && status != 3) ||
        (waited == second && status != 5) || waitpid(-1, &status, WNOHANG) != -1) {
        return -1;
    }
    return 0;
}

static int test_process_slot_reuse(void)
{
    for (int round = 0; round < 40; round++) {
        int status = -1;
        pid_t child = fork();

        if (child == 0) _exit(round & 7);
        if (child < 0 || waitpid(child, &status, 0) != child ||
            status != (round & 7)) {
            return -1;
        }
    }
    return 0;
}

static void fault_guard(void)
{
    *(volatile unsigned char *)(uintptr_t)EXEC_STACK_GUARD_ADDRESS = 1u;
}

__attribute__((noinline)) static void fault_over_cap(unsigned depth)
{
    volatile unsigned char storage[8192];

    storage[0] = (unsigned char)depth;
    storage[sizeof(storage) - 1u] = (unsigned char)(depth + 1u);
    if (depth != 0u) fault_over_cap(depth - 1u);
    if (storage[0] == 0xffu) _exit(storage[sizeof(storage) - 1u]);
}

static void fault_protection(void)
{
    *(volatile unsigned char *)(uintptr_t)&fault_protection = 0u;
}

static int expect_fault(void (*trigger)(void))
{
    int status = -1;
    pid_t child = fork();

    if (child == 0) {
        trigger();
        _exit(99);
    }
    return child > 0 && waitpid(child, &status, 0) == child &&
                   status == EXPECTED_FAULT_STATUS
               ? 0
               : -1;
}

static void trigger_cap(void)
{
    fault_over_cap(180u);
}

int main(void)
{
    uint64_t value = deep_stack(192u, 0x1234u);

    if (value == 0u) return fail("deep stack result");
    if (test_parent_waits_first() != 0) return fail("parent waits first");
    if (test_pipe_eof_then_wait() != 0) return fail("pipe EOF/delayed wait/repeat");
    if (test_nohang_and_wait_any() != 0) return fail("WNOHANG/wait any");
    if (test_process_slot_reuse() != 0) return fail("process slot reuse");
    if (fork_on_deep_stack(96u) != 0) return fail("fork after stack growth");
    if (expect_fault(fault_guard) != 0) return fail("guard fault");
    if (expect_fault(trigger_cap) != 0) return fail("stack cap fault");
    if (expect_fault(fault_protection) != 0) return fail("protection fault");

    puts("[process_stack_test] PASS");
    return 0;
}
