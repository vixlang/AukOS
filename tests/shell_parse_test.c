#include <aukos/shell_parse.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static void expect_parse(char *line, size_t line_capacity,
                         const char *const *expected, size_t expected_argc)
{
    char *argv[SHELL_PARSE_MAX_ARGS + 1];
    size_t argc = 99;
    enum shell_parse_result result;

    result = shell_parse_line(line, line_capacity, argv, ARRAY_SIZE(argv), &argc);
    assert(result == SHELL_PARSE_OK);
    assert(argc == expected_argc);
    for (size_t index = 0; index < argc; index++) {
        assert(strcmp(argv[index], expected[index]) == 0);
    }
    assert(argv[argc] == NULL);
}

static void expect_failure(char *line, size_t line_capacity,
                           size_t argv_capacity,
                           enum shell_parse_result expected_result)
{
    char *argv[SHELL_PARSE_MAX_ARGS + 1];
    size_t argc = 99;
    enum shell_parse_result result;

    for (size_t index = 0; index < ARRAY_SIZE(argv); index++) {
        argv[index] = (char *)"partial";
    }
    result = shell_parse_line(line, line_capacity, argv, argv_capacity, &argc);
    assert(result == expected_result);
    assert(argc == 0);
    if (argv_capacity != 0) {
        assert(argv[0] == NULL);
    }
}

static void expect_pipeline(char *line, size_t expected_stages)
{
    struct shell_pipeline pipeline;

    assert(shell_parse_pipeline(line, strlen(line) + 1u, &pipeline) ==
           SHELL_PARSE_OK);
    assert(pipeline.stage_count == expected_stages);
}

static void expect_pipeline_failure(char *line,
                                    enum shell_parse_result expected)
{
    struct shell_pipeline pipeline;

    assert(shell_parse_pipeline(line, strlen(line) + 1u, &pipeline) ==
           expected);
    assert(pipeline.stage_count == 0u);
}

int main(void)
{
    {
        char line[] = "toybox echo one two";
        const char *expected[] = {"toybox", "echo", "one", "two"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "  toybox   echo\t one\t\ttwo  ";
        const char *expected[] = {"toybox", "echo", "one", "two"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "toybox echo 'single quoted argument'";
        const char *expected[] = {"toybox", "echo", "single quoted argument"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "toybox echo \"double quoted argument\"";
        const char *expected[] = {"toybox", "echo", "double quoted argument"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "echo escaped\\ space \\\"quote\\\" slash\\\\end";
        const char *expected[] = {"echo", "escaped space", "\"quote\"", "slash\\end"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "echo \"double \\\"quote\\\" and \\\\ slash\"";
        const char *expected[] = {"echo", "double \"quote\" and \\ slash"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "echo 'single \\ stays'";
        const char *expected[] = {"echo", "single \\ stays"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "echo \"\" tail";
        const char *expected[] = {"echo", "", "tail"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "a b c d e f g h i j k l m n o";
        const char *expected[] = {"a", "b", "c", "d", "e", "f", "g", "h",
                                  "i", "j", "k", "l", "m", "n", "o"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "a b c d e f g h i j k l m n o p";
        const char *expected[] = {"a", "b", "c", "d", "e", "f", "g", "h",
                                  "i", "j", "k", "l", "m", "n", "o", "p"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[] = "a b c d e f g h i j k l m n o p q";
        expect_failure(line, sizeof(line), SHELL_PARSE_MAX_ARGS + 1,
                       SHELL_PARSE_ARGV_OVERFLOW);
    }
    {
        char line[] = "one two three";
        expect_failure(line, sizeof(line), 3, SHELL_PARSE_ARGV_OVERFLOW);
    }
    {
        char line[] = "echo 'missing";
        expect_failure(line, sizeof(line), SHELL_PARSE_MAX_ARGS + 1,
                       SHELL_PARSE_UNMATCHED_SINGLE_QUOTE);
    }
    {
        char line[] = "echo \"missing";
        expect_failure(line, sizeof(line), SHELL_PARSE_MAX_ARGS + 1,
                       SHELL_PARSE_UNMATCHED_DOUBLE_QUOTE);
    }
    {
        char line[] = "echo trailing\\";
        expect_failure(line, sizeof(line), SHELL_PARSE_MAX_ARGS + 1,
                       SHELL_PARSE_TRAILING_BACKSLASH);
    }
    {
        const char operators[] = "|&;< >";
        for (size_t index = 0; operators[index] != '\0'; index++) {
            char line[] = "echo X";
            if (operators[index] == ' ') {
                continue;
            }
            line[5] = operators[index];
            expect_failure(line, sizeof(line), SHELL_PARSE_MAX_ARGS + 1,
                           SHELL_PARSE_UNQUOTED_OPERATOR);
        }
    }
    {
        char line[] = "echo '|' \"&\" \\; \\< \\>";
        const char *expected[] = {"echo", "|", "&", ";", "<", ">"};
        expect_parse(line, sizeof(line), expected, ARRAY_SIZE(expected));
    }
    {
        char line[4] = {'a', 'b', 'c', 'd'};
        expect_failure(line, sizeof(line), SHELL_PARSE_MAX_ARGS + 1,
                       SHELL_PARSE_UNTERMINATED_LINE);
    }
    {
        char line[] = " \t ";
        const char **expected = NULL;
        expect_parse(line, sizeof(line), expected, 0);
    }
    {
        char line[] = "printf 'pipeline-ok\\n'|wc -c > /work/count";
        struct shell_pipeline pipeline;

        assert(shell_parse_pipeline(line, sizeof(line), &pipeline) ==
               SHELL_PARSE_OK);
        assert(pipeline.stage_count == 2u);
        assert(pipeline.stages[0].argc == 2u);
        assert(strcmp(pipeline.stages[0].argv[1], "pipeline-ok\\n") == 0);
        assert(pipeline.stages[1].argc == 2u);
        assert(strcmp(pipeline.stages[1].argv[0], "wc") == 0);
        assert(strcmp(pipeline.stages[1].output_path, "/work/count") == 0);
        assert(pipeline.stages[1].output_append == 0);
    }
    {
        char line[] = "cat < input | echo '>' \\| >>output";
        struct shell_pipeline pipeline;

        assert(shell_parse_pipeline(line, sizeof(line), &pipeline) ==
               SHELL_PARSE_OK);
        assert(pipeline.stage_count == 2u);
        assert(strcmp(pipeline.stages[0].input_path, "input") == 0);
        assert(strcmp(pipeline.stages[1].argv[1], ">") == 0);
        assert(strcmp(pipeline.stages[1].argv[2], "|") == 0);
        assert(pipeline.stages[1].output_append == 1);
    }
    {
        char line[] = "| echo x";
        expect_pipeline_failure(line, SHELL_PARSE_EMPTY_STAGE);
    }
    {
        char line[] = "echo x |";
        expect_pipeline_failure(line, SHELL_PARSE_EMPTY_STAGE);
    }
    {
        char line[] = "echo >";
        expect_pipeline_failure(line, SHELL_PARSE_MISSING_REDIRECTION_PATH);
    }
    {
        char line[] = "echo > one >> two";
        expect_pipeline_failure(line, SHELL_PARSE_CONFLICTING_REDIRECTION);
    }
    {
        char line[] = "echo x && echo y";
        expect_pipeline_failure(line, SHELL_PARSE_UNSUPPORTED_OPERATOR);
    }
    {
        char line[] = "a|b|c|d|e|f|g|h|i";
        expect_pipeline_failure(line, SHELL_PARSE_PIPELINE_OVERFLOW);
    }
    {
        char line[] = "echo one | cat";
        expect_pipeline(line, 2u);
    }

    puts("shell_parse_test: PASS");
    return 0;
}
