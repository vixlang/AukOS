#ifndef _SYS_PERSONALITY_H
#define _SYS_PERSONALITY_H

#ifdef __cplusplus
extern "C" {
#endif

#define PER_LINUX 0
#define PER_LINUX32 8

int personality(unsigned long persona);

#ifdef __cplusplus
}
#endif

#endif