#ifndef AUKOS_LOCALE_H
#define AUKOS_LOCALE_H

#define LC_ALL 0
#define LC_CTYPE 1
#define LC_NUMERIC 2
#define LC_TIME 3
#define LC_COLLATE 4
#define LC_MONETARY 5
#define LC_MESSAGES 6

#define LC_CTYPE_MASK (1 << LC_CTYPE)

typedef void *locale_t;

char *setlocale(int category, const char *locale);
locale_t newlocale(int category_mask, const char *locale, locale_t base);
locale_t uselocale(locale_t locale);
void freelocale(locale_t locale);

#endif
