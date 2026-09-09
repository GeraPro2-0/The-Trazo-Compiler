/*
 * Copyright 2026 GeraPro2_0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * This uses the LLVM Exception.
 */

#include "radar.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define radar_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define radar_mkdir(path) mkdir(path, 0755)
#endif

static int append_text(char *buffer, size_t capacity, size_t *length, const char *text) {
    size_t text_length = strlen(text);
    if (*length + text_length + 1 > capacity) return 0;
    memcpy(buffer + *length, text, text_length);
    *length += text_length;
    buffer[*length] = '\0';
    return 1;
}

static int append_argument(char *buffer, size_t capacity, size_t *length, const char *argument) {
    const char *cursor;
    if (!append_text(buffer, capacity, length, "\"")) return 0;
    for (cursor = argument; *cursor; ++cursor) {
        if (*cursor == '"' || *cursor == '\\') {
            if (!append_text(buffer, capacity, length, "\\")) return 0;
        }
        if (*length + 2 > capacity) return 0;
        buffer[(*length)++] = *cursor;
        buffer[*length] = '\0';
    }
    return append_text(buffer, capacity, length, "\"");
}

static char *project_directory(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *separator = slash;
    size_t length;
    char *directory;
    if (backslash && (!separator || backslash > separator)) separator = backslash;
    length = separator ? (size_t)(separator - path) : 1;
    directory = malloc(length + 1);
    if (!directory) return NULL;
    if (separator) memcpy(directory, path, length);
    else directory[0] = '.';
    directory[length] = '\0';
    return directory;
}

static char *relative_path(const char *directory, const char *path) {
    size_t directory_length = strlen(directory);
    size_t path_length = strlen(path);
    char *result = malloc(directory_length + path_length + 2);
    if (!result) return NULL;
    memcpy(result, directory, directory_length);
    result[directory_length] = '/';
    memcpy(result + directory_length + 1, path, path_length + 1);
    return result;
}

static int ensure_output_directory(const char *output) {
    char *copy = malloc(strlen(output) + 1);
    char *slash;
    int result;
    if (!copy) return 0;
    strcpy(copy, output);
    slash = strrchr(copy, '/');
    if (!slash) slash = strrchr(copy, '\\');
    if (!slash) {
        free(copy);
        return 1;
    }
    *slash = '\0';
    result = radar_mkdir(copy) == 0 || errno == EEXIST;
    free(copy);
    return result;
}

static int valid_compiler(const char *compiler) {
    return !compiler || strcmp(compiler, "auto") == 0
        || strcmp(compiler, "gcc") == 0
        || strcmp(compiler, "clang") == 0
        || strcmp(compiler, "msvc") == 0;
}

static char *read_source(const char *path) {
    FILE *file = fopen(path, "rb");
    long size;
    char *source;
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        if (file) fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return NULL; }
    source = malloc((size_t)size + 1);
    if (!source || fread(source, 1, (size_t)size, file) != (size_t)size) {
        free(source); fclose(file); return NULL;
    }
    source[size] = '\0';
    fclose(file);
    return source;
}

static int run_build(const RadarBuildPlan *plan, const char *project_path) {
    const char *compiler = plan->compiler;
    const char *output_name = plan->output;
    char *directory = project_directory(project_path);
    char *output = NULL;
    char command[32768] = {0};
    size_t command_length = 0;
    size_t index;
    int status;
    if (!directory) return 0;
    if (!valid_compiler(compiler)) {
        fprintf(stderr, "radar: invalid compiler '%s' (expected auto, gcc, clang, or msvc)\n",
                compiler);
        free(directory);
        return 0;
    }
    if (!compiler || strcmp(compiler, "auto") == 0) {
        compiler = getenv("CC");
        if (!compiler || !*compiler) compiler = "gcc";
    }
    if (!output_name) {
#ifdef _WIN32
        output_name = "bin/Trazo.exe";
#else
        output_name = "bin/Trazo";
#endif
    }
    if (strcmp(plan->kind ? plan->kind : "program", "program") != 0) {
        fprintf(stderr, "radar: MVP only supports build kind 'program'\n");
        free(directory);
        return 0;
    }
    output = relative_path(directory, output_name);
    if (!output || !ensure_output_directory(output)) {
        fprintf(stderr, "radar: unable to create output directory\n");
        free(output);
        free(directory);
        return 0;
    }
    if (!append_text(command, sizeof(command), &command_length, compiler)) goto overflow;
    if (plan->standard) {
        if (!append_text(command, sizeof(command), &command_length, " -std=")) goto overflow;
        if (!append_argument(command, sizeof(command), &command_length, plan->standard)) goto overflow;
    }
    for (index = 0; index < plan->include_count; ++index) {
        char *include = relative_path(directory, plan->includes[index]);
        if (!include || !append_text(command, sizeof(command), &command_length, " -I")
            || !append_argument(command, sizeof(command), &command_length, include)) {
            free(include);
            goto overflow;
        }
        free(include);
    }
    for (index = 0; index < plan->flag_count; ++index) {
        if (!append_text(command, sizeof(command), &command_length, " ")
            || !append_argument(command, sizeof(command), &command_length, plan->flags[index])) goto overflow;
    }
    for (index = 0; index < plan->source_count; ++index) {
        char *source = relative_path(directory, plan->sources[index]);
        if (!source || !append_text(command, sizeof(command), &command_length, " ")
            || !append_argument(command, sizeof(command), &command_length, source)) {
            free(source);
            goto overflow;
        }
        free(source);
    }
    if (plan->source_count == 0) {
        fprintf(stderr, "radar: build plan contains no sources\n");
        free(output);
        free(directory);
        return 0;
    }
    if (!append_text(command, sizeof(command), &command_length, " -o")
        || !append_argument(command, sizeof(command), &command_length, output)) goto overflow;
    printf("radar: %s\n", command);
    status = system(command);
    free(output);
    free(directory);
    if (status != 0) {
        fprintf(stderr, "radar: compiler failed with status %d\n", status);
        return 0;
    }
    return 1;

overflow:
    fprintf(stderr, "radar: build command is too long\n");
    free(output);
    free(directory);
    return 0;
}

int main(int argc, char **argv) {
    RadarTokenList tokens = {0};
    RadarLexerError lexer_error = {0};
    RadarParserError error;
    RadarValidationError validation_error;
    RadarBuildPlan plan = {0};
    RadarPlanError plan_error;
    char *source;
    const char *path;
    int show_tokens = argc == 3 && strcmp(argv[1], "--tokens") == 0;
    int show_plan = argc == 3 && strcmp(argv[1], "plan") == 0;
    int do_build = argc == 3 && strcmp(argv[1], "build") == 0;
    if (argc == 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: radar check Radar.toml\n");
        printf("       radar plan Radar.toml\n");
        printf("       radar build Radar.toml\n");
        printf("       radar --tokens Radar.toml\n");
        printf("       radar Radar.toml\n");
        return 0;
    }
    if (argc == 3 && (strcmp(argv[1], "check") == 0 || show_tokens || show_plan || do_build)) path = argv[2];
    else if (argc == 2) path = argv[1];
    else {
        fprintf(stderr, "Usage: radar check Radar.toml\n");
        return 1;
    }
    source = read_source(path);
    if (!source) {
        fprintf(stderr, "radar: unable to read %s\n", path);
        free(source); radar_tokens_free(&tokens); return 1;
    }
    if (!radar_lex(source, &tokens, &lexer_error)) {
        fprintf(stderr, "radar: %s:%zu:%zu: %s\n", path, lexer_error.line,
                lexer_error.column, lexer_error.message ? lexer_error.message : "lexical error");
        free(source); radar_tokens_free(&tokens); return 1;
    }
    if (show_tokens) {
        size_t index;
        for (index = 0; index < tokens.count; ++index) {
            RadarToken *token = &tokens.items[index];
            printf("%zu:%zu %-24s %.*s\n", token->line, token->column,
                   radar_token_name(token->kind), (int)token->length, token->start);
        }
        free(source);
        radar_tokens_free(&tokens);
        return 0;
    }
    if (!radar_validate_tokens(&tokens, &validation_error)) {
        fprintf(stderr, "radar: %s:%zu:%zu: %s\n", path, validation_error.line,
                validation_error.column, validation_error.message);
        free(source); radar_tokens_free(&tokens); return 1;
    }
    if (!radar_parse(&tokens, &error)) {
        fprintf(stderr, "radar: %s:%zu:%zu: %s\n", path, error.line, error.column, error.message);
        free(source); radar_tokens_free(&tokens); return 1;
    }
    if (show_plan || do_build) {
        if (!radar_build_plan(&tokens, &plan, &plan_error)) {
            fprintf(stderr, "radar: %s:%zu:%zu: %s\n", path, plan_error.line,
                    plan_error.column, plan_error.message);
            free(source); radar_tokens_free(&tokens); return 1;
        }
        if (!valid_compiler(plan.compiler)) {
            fprintf(stderr, "radar: %s: invalid compiler '%s'\n", path, plan.compiler);
            radar_build_plan_free(&plan);
            free(source); radar_tokens_free(&tokens); return 1;
        }
        printf("compiler = %s\n", plan.compiler ? plan.compiler : "auto");
        printf("kind = %s\n", plan.kind ? plan.kind : "program");
        printf("output = %s\n", plan.output ? plan.output : "(default)");
        printf("standard = %s\n", plan.standard ? plan.standard : "(default)");
        printf("includes = %zu\n", plan.include_count);
        printf("sources = %zu\n", plan.source_count);
        printf("flags = %zu\n", plan.flag_count);
        if (do_build && !run_build(&plan, path)) {
            radar_build_plan_free(&plan);
            free(source); radar_tokens_free(&tokens); return 1;
        }
        radar_build_plan_free(&plan);
    }
    printf("radar: %s is valid\n", path);
    free(source); radar_tokens_free(&tokens);
    return 0;
}
