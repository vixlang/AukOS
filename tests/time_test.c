#include <assert.h>
#include <string.h>
#include <time.h>

int main(void)
{
    char buffer[64];
    struct tm value;
    time_t epoch = 0;
    time_t leap_day = 951827696;

    assert(gmtime(&epoch) != 0);
    value = *gmtime(&epoch);
    assert(value.tm_year == 70);
    assert(value.tm_mon == 0);
    assert(value.tm_mday == 1);
    assert(value.tm_wday == 4);
    assert(value.tm_yday == 0);
    assert(strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %% %j %w", &value) == 27u);
    assert(strcmp(buffer, "1970-01-01 00:00:00 % 001 4") == 0);

    assert(gmtime(&leap_day) != 0);
    value = *gmtime(&leap_day);
    assert(strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &value) == 19u);
    assert(strcmp(buffer, "2000-02-29 12:34:56") == 0);

    strcpy(buffer, "unchanged");
    assert(strftime(buffer, 19u, "%Y-%m-%d %H:%M:%S", &value) == 0u);
    assert(buffer[0] == '\0');
    assert(strftime(buffer, sizeof(buffer), "%Y", 0) == 0u);
    return 0;
}
