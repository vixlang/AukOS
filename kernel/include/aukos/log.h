#ifndef AUKOS_LOG_H
#define AUKOS_LOG_H

enum log_level {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
};

void log_set_level(enum log_level level);
void log_write(enum log_level level, const char *message);

#define log_debug(message) log_write(LOG_DEBUG, message)
#define log_info(message) log_write(LOG_INFO, message)
#define log_warn(message) log_write(LOG_WARN, message)
#define log_error(message) log_write(LOG_ERROR, message)

#endif
