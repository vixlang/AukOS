#ifndef AUKOS_SETJMP_H
#define AUKOS_SETJMP_H

typedef struct { unsigned long regs[8]; } jmp_buf[1];
typedef jmp_buf sigjmp_buf;

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int value) __attribute__((noreturn));
int sigsetjmp(sigjmp_buf env, int savesigs);
void siglongjmp(sigjmp_buf env, int value) __attribute__((noreturn));

#endif
