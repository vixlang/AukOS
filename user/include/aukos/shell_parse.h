#ifndef AUKOS_SHELL_PARSE_H
#define AUKOS_SHELL_PARSE_H

#include <stddef.h>

#define SHELL_PARSE_MAX_ARGS 16u
#define SHELL_PARSE_MAX_STAGES 8u

struct shell_command {
    char *argv[SHELL_PARSE_MAX_ARGS + 1u];
    size_t argc;
    char *input_path;
    char *output_path;
    int output_append;
};

struct shell_pipeline {
    struct shell_command stages[SHELL_PARSE_MAX_STAGES];
    size_t stage_count;
};

enum shell_parse_result {
    SHELL_PARSE_OK = 0,
    SHELL_PARSE_UNTERMINATED_LINE,
    SHELL_PARSE_ARGV_OVERFLOW,
    SHELL_PARSE_UNMATCHED_SINGLE_QUOTE,
    SHELL_PARSE_UNMATCHED_DOUBLE_QUOTE,
    SHELL_PARSE_TRAILING_BACKSLASH,
    SHELL_PARSE_UNQUOTED_OPERATOR,
    SHELL_PARSE_PIPELINE_OVERFLOW,
    SHELL_PARSE_EMPTY_STAGE,
    SHELL_PARSE_MISSING_REDIRECTION_PATH,
    SHELL_PARSE_CONFLICTING_REDIRECTION,
    SHELL_PARSE_UNSUPPORTED_OPERATOR,
};

enum shell_parse_result shell_parse_pipeline(char *line, size_t line_capacity,
                                              struct shell_pipeline *pipeline);

enum shell_parse_result shell_parse_line(char *line, size_t line_capacity,
                                         char **argv, size_t argv_capacity,
                                         size_t *argc_out);
const char *shell_parse_result_string(enum shell_parse_result result);

#endif
