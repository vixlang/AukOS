#include "include/aukos/log.h"

#include "include/aukos/console.h"
#include "include/aukos/serial.h"

static enum log_level current_level = LOG_DEBUG;

static const char *level_name(enum log_level level)
{
    switch (level) {
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_INFO:
        return "INFO";
    case LOG_WARN:
        return "WARN";
    case LOG_ERROR:
        return "ERROR";
    }

    return "UNKNOWN";
}

static const char *level_prefix(enum log_level level)
{
    if (level == LOG_INFO) {
        return "OK";
    }

    return level_name(level);
}

void log_set_level(enum log_level level)
{
    current_level = level;
}

void log_write(enum log_level level, const char *message)
{
    const char *name;

    if (level < current_level) {
        return;
    }

    name = level_prefix(level);

    if (level == LOG_INFO) {
        serial_write("\033[32m");
    }
    serial_write("[");
    serial_write(name);
    serial_write("] ");
    if (level == LOG_INFO) {
        serial_write("\033[0m");
    }
    serial_write_line(message);

    if (level == LOG_INFO) {
        console_write_color("[OK] ", 0x0a);
    } else {
        console_write("[");
        console_write(name);
        console_write("] ");
    }
    console_write_line(message);
}
