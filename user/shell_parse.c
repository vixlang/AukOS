#include <aukos/shell_parse.h>

enum quote_state {
    QUOTE_NONE = 0,
    QUOTE_SINGLE,
    QUOTE_DOUBLE,
};

enum token_type {
    TOKEN_WORD = 0,
    TOKEN_PIPE,
    TOKEN_INPUT,
    TOKEN_OUTPUT,
    TOKEN_APPEND,
};

struct token {
    enum token_type type;
    char *word;
};

#define SHELL_PARSE_MAX_TOKENS \
    (SHELL_PARSE_MAX_STAGES * (SHELL_PARSE_MAX_ARGS + 4u))

static int is_separator(char value)
{
    return value == ' ' || value == '\t';
}

static void clear_pipeline(struct shell_pipeline *pipeline)
{
    pipeline->stage_count = 0;
    for (size_t stage = 0; stage < SHELL_PARSE_MAX_STAGES; stage++) {
        pipeline->stages[stage].argc = 0;
        pipeline->stages[stage].argv[0] = 0;
        pipeline->stages[stage].input_path = 0;
        pipeline->stages[stage].output_path = 0;
        pipeline->stages[stage].output_append = 0;
    }
}

static enum shell_parse_result add_token(struct token *tokens,
                                         size_t *token_count,
                                         enum token_type type, char *word)
{
    if (*token_count == SHELL_PARSE_MAX_TOKENS) {
        return SHELL_PARSE_ARGV_OVERFLOW;
    }
    tokens[*token_count].type = type;
    tokens[*token_count].word = word;
    (*token_count)++;
    return SHELL_PARSE_OK;
}

static enum shell_parse_result lex_line(char *line, size_t line_capacity,
                                        struct token *tokens,
                                        size_t *token_count)
{
    char *read_cursor = line;
    char *write_cursor = line;
    char *line_end = 0;
    char *word_start = 0;
    enum quote_state quote = QUOTE_NONE;
    int word_active = 0;

    *token_count = 0;
    for (size_t index = 0; index < line_capacity; index++) {
        if (line[index] == '\0') {
            line_end = line + index;
            break;
        }
    }
    if (!line_end) {
        return SHELL_PARSE_UNTERMINATED_LINE;
    }

    while (read_cursor < line_end) {
        char value = *read_cursor++;

        if (quote == QUOTE_SINGLE) {
            if (value == '\'') {
                quote = QUOTE_NONE;
            } else {
                *write_cursor++ = value;
            }
            continue;
        }
        if (quote == QUOTE_DOUBLE) {
            if (value == '"') {
                quote = QUOTE_NONE;
            } else if (value == '\\') {
                if (read_cursor == line_end) {
                    return SHELL_PARSE_TRAILING_BACKSLASH;
                }
                *write_cursor++ = *read_cursor++;
            } else {
                *write_cursor++ = value;
            }
            continue;
        }

        if (value == '\'' || value == '"') {
            if (!word_active) {
                word_active = 1;
                word_start = write_cursor;
            }
            quote = value == '\'' ? QUOTE_SINGLE : QUOTE_DOUBLE;
            continue;
        }
        if (value == '\\') {
            if (read_cursor == line_end) {
                return SHELL_PARSE_TRAILING_BACKSLASH;
            }
            if (!word_active) {
                word_active = 1;
                word_start = write_cursor;
            }
            *write_cursor++ = *read_cursor++;
            continue;
        }
        if (is_separator(value) || value == '|' || value == '<' ||
            value == '>' || value == '&' || value == ';') {
            enum shell_parse_result result;

            if (word_active) {
                *write_cursor++ = '\0';
                result = add_token(tokens, token_count, TOKEN_WORD, word_start);
                if (result != SHELL_PARSE_OK) {
                    return result;
                }
                word_active = 0;
            }
            if (is_separator(value)) {
                continue;
            }
            if (value == '&' || value == ';' ||
                (value == '|' && read_cursor < line_end && *read_cursor == '|') ||
                (value == '<' && read_cursor < line_end && *read_cursor == '<')) {
                return SHELL_PARSE_UNSUPPORTED_OPERATOR;
            }
            if (value == '>') {
                if (read_cursor < line_end && *read_cursor == '>') {
                    read_cursor++;
                    result = add_token(tokens, token_count, TOKEN_APPEND, 0);
                } else {
                    result = add_token(tokens, token_count, TOKEN_OUTPUT, 0);
                }
            } else if (value == '<') {
                result = add_token(tokens, token_count, TOKEN_INPUT, 0);
            } else {
                result = add_token(tokens, token_count, TOKEN_PIPE, 0);
            }
            if (result != SHELL_PARSE_OK) {
                return result;
            }
            continue;
        }
        if (!word_active) {
            word_active = 1;
            word_start = write_cursor;
        }
        *write_cursor++ = value;
    }

    if (quote == QUOTE_SINGLE) {
        return SHELL_PARSE_UNMATCHED_SINGLE_QUOTE;
    }
    if (quote == QUOTE_DOUBLE) {
        return SHELL_PARSE_UNMATCHED_DOUBLE_QUOTE;
    }
    if (word_active) {
        *write_cursor++ = '\0';
        return add_token(tokens, token_count, TOKEN_WORD, word_start);
    }
    return SHELL_PARSE_OK;
}

enum shell_parse_result shell_parse_pipeline(char *line, size_t line_capacity,
                                              struct shell_pipeline *pipeline)
{
    struct token tokens[SHELL_PARSE_MAX_TOKENS];
    size_t token_count;
    enum token_type pending_redirection = TOKEN_WORD;
    enum shell_parse_result result;
    struct shell_command *command;

    if (!pipeline) {
        return SHELL_PARSE_ARGV_OVERFLOW;
    }
    clear_pipeline(pipeline);
    result = lex_line(line, line_capacity, tokens, &token_count);
    if (result != SHELL_PARSE_OK || token_count == 0u) {
        return result;
    }

    pipeline->stage_count = 1u;
    command = &pipeline->stages[0];
    for (size_t index = 0; index < token_count; index++) {
        struct token *token = &tokens[index];

        if (token->type == TOKEN_WORD) {
            if (pending_redirection == TOKEN_INPUT) {
                command->input_path = token->word;
                pending_redirection = TOKEN_WORD;
            } else if (pending_redirection == TOKEN_OUTPUT ||
                       pending_redirection == TOKEN_APPEND) {
                command->output_path = token->word;
                command->output_append = pending_redirection == TOKEN_APPEND;
                pending_redirection = TOKEN_WORD;
            } else {
                if (command->argc == SHELL_PARSE_MAX_ARGS) {
                    clear_pipeline(pipeline);
                    return SHELL_PARSE_ARGV_OVERFLOW;
                }
                command->argv[command->argc++] = token->word;
                command->argv[command->argc] = 0;
            }
            continue;
        }
        if (pending_redirection != TOKEN_WORD) {
            clear_pipeline(pipeline);
            return SHELL_PARSE_MISSING_REDIRECTION_PATH;
        }
        if (token->type == TOKEN_PIPE) {
            if (command->argc == 0u) {
                clear_pipeline(pipeline);
                return SHELL_PARSE_EMPTY_STAGE;
            }
            if (pipeline->stage_count == SHELL_PARSE_MAX_STAGES) {
                clear_pipeline(pipeline);
                return SHELL_PARSE_PIPELINE_OVERFLOW;
            }
            command = &pipeline->stages[pipeline->stage_count++];
            continue;
        }
        if ((token->type == TOKEN_INPUT && command->input_path) ||
            ((token->type == TOKEN_OUTPUT || token->type == TOKEN_APPEND) &&
             command->output_path)) {
            clear_pipeline(pipeline);
            return SHELL_PARSE_CONFLICTING_REDIRECTION;
        }
        pending_redirection = token->type;
    }

    if (pending_redirection != TOKEN_WORD) {
        clear_pipeline(pipeline);
        return SHELL_PARSE_MISSING_REDIRECTION_PATH;
    }
    if (command->argc == 0u) {
        clear_pipeline(pipeline);
        return SHELL_PARSE_EMPTY_STAGE;
    }
    return SHELL_PARSE_OK;
}

enum shell_parse_result shell_parse_line(char *line, size_t line_capacity,
                                         char **argv, size_t argv_capacity,
                                         size_t *argc_out)
{
    struct shell_pipeline pipeline;
    enum shell_parse_result result;

    *argc_out = 0;
    if (argv_capacity != 0u) {
        argv[0] = 0;
    }
    result = shell_parse_pipeline(line, line_capacity, &pipeline);
    if (result != SHELL_PARSE_OK) {
        if (result == SHELL_PARSE_PIPELINE_OVERFLOW ||
            result == SHELL_PARSE_EMPTY_STAGE ||
            result == SHELL_PARSE_MISSING_REDIRECTION_PATH ||
            result == SHELL_PARSE_CONFLICTING_REDIRECTION ||
            result == SHELL_PARSE_UNSUPPORTED_OPERATOR) {
            return SHELL_PARSE_UNQUOTED_OPERATOR;
        }
        return result;
    }
    if (pipeline.stage_count == 0u) {
        return SHELL_PARSE_OK;
    }
    if (pipeline.stage_count != 1u || pipeline.stages[0].input_path ||
        pipeline.stages[0].output_path) {
        return SHELL_PARSE_UNQUOTED_OPERATOR;
    }
    if (argv_capacity == 0u || pipeline.stages[0].argc + 1u > argv_capacity) {
        return SHELL_PARSE_ARGV_OVERFLOW;
    }
    for (size_t index = 0; index < pipeline.stages[0].argc; index++) {
        argv[index] = pipeline.stages[0].argv[index];
    }
    argv[pipeline.stages[0].argc] = 0;
    *argc_out = pipeline.stages[0].argc;
    return SHELL_PARSE_OK;
}

const char *shell_parse_result_string(enum shell_parse_result result)
{
    switch (result) {
    case SHELL_PARSE_OK: return "ok";
    case SHELL_PARSE_UNTERMINATED_LINE: return "line is not NUL-terminated within its buffer";
    case SHELL_PARSE_ARGV_OVERFLOW: return "too many arguments";
    case SHELL_PARSE_UNMATCHED_SINGLE_QUOTE: return "unmatched single quote";
    case SHELL_PARSE_UNMATCHED_DOUBLE_QUOTE: return "unmatched double quote";
    case SHELL_PARSE_TRAILING_BACKSLASH: return "trailing backslash";
    case SHELL_PARSE_UNQUOTED_OPERATOR: return "unsupported shell operator";
    case SHELL_PARSE_PIPELINE_OVERFLOW: return "too many pipeline stages";
    case SHELL_PARSE_EMPTY_STAGE: return "empty pipeline stage";
    case SHELL_PARSE_MISSING_REDIRECTION_PATH: return "missing redirection filename";
    case SHELL_PARSE_CONFLICTING_REDIRECTION: return "conflicting redirection";
    case SHELL_PARSE_UNSUPPORTED_OPERATOR: return "unsupported shell operator";
    }
    return "unknown parse error";
}
