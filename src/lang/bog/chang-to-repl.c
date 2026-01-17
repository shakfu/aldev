#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "bog.h"

static char* read_entire_file(const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp)
        return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    size_t read = fread(buffer, 1, (size_t)size, fp);
    buffer[read] = '\0';
    fclose(fp);
    return buffer;
}

static char* extract_query_body(char* line)
{
    if (!line)
        return NULL;
    while (isspace((unsigned char)*line))
        line++;
    if (*line == '\0')
        return NULL;
    if (line[0] == '?' && line[1] == '-') {
        line += 2;
        while (isspace((unsigned char)*line))
            line++;
    }
    if (*line == '\0')
        return NULL;
    char* body = strdup(line);
    if (!body)
        return NULL;
    size_t len = strlen(body);
    while (len > 0 && isspace((unsigned char)body[len - 1])) {
        body[--len] = '\0';
    }
    if (len == 0) {
        free(body);
        return NULL;
    }
    if (body[len - 1] == '.') {
        body[--len] = '\0';
        while (len > 0 && isspace((unsigned char)body[len - 1])) {
            body[--len] = '\0';
        }
    }
    if (len == 0) {
        free(body);
        return NULL;
    }
    return body;
}

static bool equals_ignore_case(const char* a, const char* b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool should_print_binding(const char* name)
{
    if (!name || name[0] == '\0')
        return false;
    if (name[0] == '_')
        return false;
    return strchr(name, '$') == NULL;
}

static void print_solution(size_t index, const BogEnv* env,
                           BogArena* arena)
{
    printf("[%zu] ", index + 1);
    bool printed = false;
    for (size_t i = 0; i < env->count; ++i) {
        const char* name = env->items[i].name;
        if (!should_print_binding(name))
            continue;
        BogTerm* resolved = bog_subst_term(env->items[i].value, env, arena);
        char* value = bog_term_to_string(resolved, arena);
        if (printed)
            printf(", ");
        printf("%s = %s", name, value ? value : "<error>");
        free(value);
        printed = true;
    }
    if (!printed) {
        printf("true");
    }
    printf(".\n");
}

static bool is_exit_command(const char* body)
{
    return equals_ignore_case(body, "quit") || equals_ignore_case(body, "halt")
        || equals_ignore_case(body, "exit");
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program-file> [bpm]\n", argv[0]);
        return 1;
    }
    double bpm = 120.0;
    if (argc >= 3) {
        bpm = strtod(argv[2], NULL);
        if (!(bpm > 0.0))
            bpm = 120.0;
    }

    char* program_src = read_entire_file(argv[1]);
    if (!program_src) {
        fprintf(stderr, "Failed to read %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    BogArena* program_arena = bog_arena_create();
    if (!program_arena) {
        fprintf(stderr, "Failed to allocate arena\n");
        free(program_src);
        return 1;
    }

    char* parse_error = NULL;
    BogProgram* program = bog_parse_program(program_src, program_arena,
                                                    &parse_error);
    if (!program) {
        fprintf(stderr, "Failed to parse %s: %s\n", argv[1],
                parse_error ? parse_error : "unknown error");
        free(parse_error);
        free(program_src);
        bog_arena_destroy(program_arena);
        return 1;
    }
    free(parse_error);
    free(program_src);

    BogBuiltins* builtins = bog_create_builtins(program_arena);
    BogContext ctx;
    ctx.bpm = bpm;
    ctx.state_manager = NULL;

    printf("Loaded %zu clauses from %s. Type queries ending with '.' (e.g. "
           "member(X, [1,2]).)\n",
           program->count, argv[1]);
    printf("Type 'quit.' to exit.\n");

    char* line = NULL;
    size_t linecap = 0;
    while (true) {
        printf("?- ");
        fflush(stdout);
        ssize_t linelen = getline(&line, &linecap, stdin);
        if (linelen < 0) {
            printf("\n");
            break;
        }
        size_t newline = strcspn(line, "\r\n");
        line[newline] = '\0';

        char* body = extract_query_body(line);
        if (!body)
            continue;
        if (is_exit_command(body)) {
            free(body);
            break;
        }

        size_t query_len = strlen(body) + 16;
        char* query_src = (char*)malloc(query_len);
        if (!query_src) {
            fprintf(stderr, "Out of memory\n");
            free(body);
            continue;
        }
        snprintf(query_src, query_len, "__query :- %s.\n", body);
        free(body);

        BogArena* query_arena = bog_arena_create();
        if (!query_arena) {
            fprintf(stderr, "Failed to allocate query arena\n");
            free(query_src);
            continue;
        }

        char* query_error = NULL;
        BogProgram* query_program = bog_parse_program(
            query_src, query_arena, &query_error);
        free(query_src);
        if (!query_program || query_program->count == 0) {
            fprintf(stderr, "Query parse error: %s\n",
                    query_error ? query_error : "unknown error");
            free(query_error);
            bog_arena_destroy(query_arena);
            continue;
        }
        free(query_error);

        BogGoalList goals = query_program->clauses[0].body;

        BogEnv env;
        bog_env_init(&env);
        BogSolutions solutions = { 0 };
        bog_resolve(&goals, &env, program, &ctx, builtins, &solutions,
                        query_arena);
        bog_env_free(&env);

        if (solutions.count == 0) {
            printf("false.\n");
        } else {
            for (size_t i = 0; i < solutions.count; ++i) {
                print_solution(i, &solutions.envs[i], query_arena);
                bog_env_free(&solutions.envs[i]);
            }
        }
        free(solutions.envs);
        bog_arena_destroy(query_arena);
    }

    free(line);
    bog_arena_destroy(program_arena);
    return 0;
}
