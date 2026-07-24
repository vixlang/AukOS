#ifndef AUKOS_WCHAR_H
#define AUKOS_WCHAR_H

#include <stddef.h>

typedef int wchar_t;
typedef unsigned int wint_t;
typedef struct { unsigned int state; } mbstate_t;

#define WEOF ((wint_t)-1)

int wcwidth(wchar_t wc);
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);

#endif
