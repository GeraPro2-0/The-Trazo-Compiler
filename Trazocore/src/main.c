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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "trazo.h"

typedef enum {
    TRAZO_OUTPUT_PROGRAM,
    TRAZO_OUTPUT_SHARED_LIBRARY,
    TRAZO_OUTPUT_STATIC_LIBRARY
} TrazoOutputKind;

typedef struct {
    const char *input_file;
    const char *output_file;
    const char *target_compiler;
    const char **compiler_args;
    size_t compiler_arg_count;
    size_t compiler_arg_capacity;
    char **owned_args;
    size_t owned_arg_count;
    size_t owned_arg_capacity;
    int c_only;
    int show_tokens;
    int strict;
    TrazoOutputKind output_kind;
} TrazoOptions;

typedef struct {
    const char *trazo_en_flag;
    const char *trazo_es_flag;
    const char *gcc_flag;
    const char *clang_flag;
    const char *msvc_flag;
} CompilerMapping;

typedef struct {
    const char *trazo_en_internal;
    const char *trazo_es_internal;
} InternalCommands;

static const CompilerMapping translation_table[] = {
    {"--release", "--publicar", "-O2", "-O2", "/O2"},
    {"--std-c=%s", "--std-c=%s", "-std=%s", "-std=%s", "/std:%s"},
    {"--fast", "--rapido", "-O3", "-O3", "/O2 /Ob3"},
    {"--small", "--pequeno", "-Os", "-Os", "/O1"},
    {"--tiny", "--diminuto", "-Oz", "-Oz", "/O1 /Gw"},
    {"--debug", "--depurar", "-Og -g", "-Og -g", "/Od /Zi /RTC1"},
    {"--no-optimize", "--sin-optimizar", "-O0", "-O0", "/Od"},
    {NULL, NULL, NULL, NULL, NULL}
};

static const InternalCommands internal_commands[] = {
    {"--tokens", "--tokens"},
    {"--cOnly", "--soloC"},
    {"--strict", "--estricto"},
    {"--help", "--ayuda"},
    {"--cc=%s", "--cc=%s"},
    {"--output=%s", "--salida=%s"},
    {"--program", "--programa"},
    {"--shared", "--dinamica"},
    {"--static", "--estatica"},
    {NULL, NULL}
};

typedef struct {
    const TrazoOptions *options;
    const char *output_directory;
    char *active[64];
    size_t active_count;
    char *built[64];
    size_t built_count;
    char *generated[64];
    size_t generated_count;
} ModuleBuildState;

static void print_usage(const char *program) {
    printf("Usage: %s [options] file.trz\n", program);
    printf("\nTrazo compiler options:\n");
    printf("  --release/--publicar Optimize for release\n");
    printf("  --fast/--rapido     Optimize for speed\n");
    printf("  --small/--pequeno   Optimize for size\n");
    printf("  --debug/--depurar   Optimize for debugging\n");
    printf("  --cOnly/--soloC     Generate only C output\n");
    printf("  --tokens/--tokens   Print lexer tokens\n");
    printf("  --strict/--estricto Enable Trazo type checking\n");
    printf("  --cc=<name>   Select GCC, Clang, or MSVC\n");
    printf("  --std-c=<version>    Select the C language standard\n");
    printf("  --output=<path>      Select the output artifact path\n");
    printf("  --program/--programa Build an executable program\n");
    printf("  --shared/--dinamica  Build a shared library\n");
    printf("  --static/--estatica  Build a static library\n");
    printf("  --help/--ayuda      Show this help\n");
    printf("\nForwarded C compiler options:\n");
    printf("  -std-c=<standard>     Select the C language standard\n");
    printf("  -O<level>           Select optimization level\n");
    printf("  -I<path> -L<path>   Add include or library search path\n");
    printf("  -l<library>         Link a library\n");
}

static const CompilerMapping *find_compiler_command(const char *argument) {
    size_t index;
    for (index = 0; index < sizeof(translation_table) / sizeof(translation_table[0]); ++index) {
        if ((translation_table[index].trazo_en_flag
                && strcmp(argument, translation_table[index].trazo_en_flag) == 0)
            || (translation_table[index].trazo_es_flag
                && strcmp(argument, translation_table[index].trazo_es_flag) == 0)) {
            return &translation_table[index];
        }
    }
    return NULL;
}

static int is_std_c_command(const char *argument) {
    return strncmp(argument, "--std-c=", 8) == 0 && argument[8] != '\0';
}

static int is_output_command(const char *argument) {
    return (strncmp(argument, "--output=", 9) == 0 && argument[9] != '\0')
        || (strncmp(argument, "--salida=", 9) == 0 && argument[9] != '\0');
}

static const InternalCommands *find_internal_command(const char *argument) {
    size_t index;
    for (index = 0; index < sizeof(internal_commands) / sizeof(internal_commands[0]); ++index) {
        if ((internal_commands[index].trazo_en_internal
                && strcmp(argument, internal_commands[index].trazo_en_internal) == 0)
            || (internal_commands[index].trazo_es_internal
                && strcmp(argument, internal_commands[index].trazo_es_internal) == 0)) {
            return &internal_commands[index];
        }
    }
    return NULL;
}

static const char *mapped_argument(const CompilerMapping *command, const char *compiler) {
    if (!compiler || strcmp(compiler, "gcc") == 0) return command->gcc_flag;
    if (strcmp(compiler, "clang") == 0) return command->clang_flag;
    if (strcmp(compiler, "msvc") == 0 || strcmp(compiler, "cl") == 0) return command->msvc_flag;
    return NULL;
}

static int append_compiler_arg(TrazoOptions *options, const char *argument) {
    if (options->compiler_arg_count == options->compiler_arg_capacity) {
        size_t capacity = options->compiler_arg_capacity == 0
            ? 8
            : options->compiler_arg_capacity * 2;
        const char **arguments = realloc(
            options->compiler_args,
            capacity * sizeof(*arguments)
        );
        if (!arguments) {
            return 0;
        }
        options->compiler_args = arguments;
        options->compiler_arg_capacity = capacity;
    }

    options->compiler_args[options->compiler_arg_count++] = argument;
    return 1;
}

static int append_owned_compiler_arg(TrazoOptions *options, const char *argument) {
    char **items;
    size_t capacity;
    if (options->owned_arg_count == options->owned_arg_capacity) {
        capacity = options->owned_arg_capacity == 0 ? 4 : options->owned_arg_capacity * 2;
        items = realloc(options->owned_args, capacity * sizeof(*items));
        if (!items) return 0;
        options->owned_args = items;
        options->owned_arg_capacity = capacity;
    }
    options->owned_args[options->owned_arg_count++] = (char *)argument;
    return append_compiler_arg(options, argument);
}

static int append_compiler_arguments(TrazoOptions *options, const char *text) {
    const char *cursor = text;
    while (*cursor) {
        const char *start;
        size_t length;
        char *argument;
        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        if (!*cursor) break;
        start = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t') ++cursor;
        length = (size_t)(cursor - start);
        argument = malloc(length + 1);
        if (!argument) return 0;
        memcpy(argument, start, length);
        argument[length] = '\0';
        if (!append_owned_compiler_arg(options, argument)) {
            free(argument);
            return 0;
        }
    }
    return 1;
}

static char *map_std_c(const char *version, const char *compiler) {
    const char *prefix = (!compiler || strcmp(compiler, "gcc") == 0
                          || strcmp(compiler, "clang") == 0) ? "-std=" : "/std:";
    size_t prefix_length = strlen(prefix);
    size_t version_length = strlen(version);
    char *mapped = malloc(prefix_length + version_length + 1);
    if (!mapped) return NULL;
    memcpy(mapped, prefix, prefix_length);
    memcpy(mapped + prefix_length, version, version_length + 1);
    if (compiler && (strcmp(compiler, "msvc") == 0 || strcmp(compiler, "cl") == 0)
        && strcmp(version, "c11") != 0 && strcmp(version, "c17") != 0) {
        fprintf(stderr, "Trazo: warning: MSVC support for C standard %s may be unavailable\n", version);
    }
    return mapped;
}

static void free_compiler_args(TrazoOptions *options) {
    size_t index;
    for (index = 0; index < options->owned_arg_count; ++index) free(options->owned_args[index]);
    free(options->owned_args);
    free(options->compiler_args);
}

static int is_forwarded_option(const char *argument) {
    return strncmp(argument, "-std=", 5) == 0
        || strncmp(argument, "-O", 2) == 0
        || strncmp(argument, "-I", 2) == 0
        || strncmp(argument, "-L", 2) == 0
        || strncmp(argument, "-l", 2) == 0
        || strcmp(argument, "-Wall") == 0
        || strcmp(argument, "-Wextra") == 0
        || strcmp(argument, "-g") == 0;
}

static int parse_options(int argc, char **argv, TrazoOptions *options) {
    for (int index = 1; index < argc; ++index) {
        if (strncmp(argv[index], "--compiler=", 11) == 0) options->target_compiler = argv[index] + 11;
        else if (strncmp(argv[index], "--cc=", 5) == 0) options->target_compiler = argv[index] + 5;
    }
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        const CompilerMapping *compiler_command;
        const InternalCommands *internal_command;

        internal_command = find_internal_command(argument);
        if (internal_command) {
            if (strcmp(internal_command->trazo_en_internal, "--help") == 0
                || strcmp(internal_command->trazo_es_internal, "--ayuda") == 0) {
                print_usage(argv[0]);
                return 0;
            }
            if (strcmp(internal_command->trazo_en_internal, "--tokens") == 0) options->show_tokens = 1;
            else if (strcmp(internal_command->trazo_en_internal, "--strict") == 0) options->strict = 1;
            else if (strcmp(internal_command->trazo_en_internal, "--cOnly") == 0) options->c_only = 1;
            else if (strcmp(internal_command->trazo_en_internal, "--program") == 0) {
                options->output_kind = TRAZO_OUTPUT_PROGRAM;
            } else if (strcmp(internal_command->trazo_en_internal, "--shared") == 0) {
                options->output_kind = TRAZO_OUTPUT_SHARED_LIBRARY;
            } else if (strcmp(internal_command->trazo_en_internal, "--static") == 0) {
                options->output_kind = TRAZO_OUTPUT_STATIC_LIBRARY;
            }
            continue;
        }
        compiler_command = find_compiler_command(argument);
        if (is_std_c_command(argument)) {
            char *mapped = map_std_c(argument + 8, options->target_compiler);
            if (!mapped || !append_compiler_arguments(options, mapped)) {
                free(mapped);
                fprintf(stderr, "Trazo: unable to store C standard option\n");
                return -1;
            }
            free(mapped);
            continue;
        }
        if (is_output_command(argument)) {
            options->output_file = argument + 9;
            continue;
        }
        if (compiler_command) {
            const char *mapped = mapped_argument(compiler_command, options->target_compiler);
            if (mapped && !append_compiler_arguments(options, mapped)) {
                fprintf(stderr, "Trazo: unable to store compiler option\n");
                return -1;
            }
            continue;
        }
        if (strncmp(argument, "--compiler=", 11) == 0) {
            options->target_compiler = argument + 11;
            continue;
        }
        if (strncmp(argument, "--cc=", 5) == 0) {
            options->target_compiler = argument + 5;
            continue;
        }
        if (strcmp(argument, "-o") == 0
            || strcmp(argument, "--output") == 0
            || strcmp(argument, "--salida") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "Trazo: output option requires a path\n");
                return -1;
            }
            options->output_file = argv[++index];
            continue;
        }
        if (strncmp(argument, "-cc=", 4) == 0) {
            options->target_compiler = argument + 4;
            continue;
        }
        if (is_forwarded_option(argument)) {
            if (!append_compiler_arg(options, argument)) {
                fprintf(stderr, "Trazo: unable to store compiler option\n");
                return -1;
            }
            continue;
        }
        if (argument[0] == '-') {
            fprintf(stderr, "Trazo: unknown option: %s\n", argument);
            return -1;
        }
        if (options->input_file) {
            fprintf(stderr, "Trazo: only one input file is supported\n");
            return -1;
        }
        options->input_file = argument;
    }

    if (!options->input_file) {
        fprintf(stderr, "Trazo: no input file\n");
        return -1;
    }
    return 1;
}

static char *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    char *source;
    long size;
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return NULL; }
    source = malloc((size_t)size + 1);
    if (!source || fread(source, 1, (size_t)size, file) != (size_t)size) {
        free(source);
        fclose(file);
        return NULL;
    }
    fclose(file);
    source[size] = '\0';
    *length = (size_t)size;
    return source;
}

static char *default_output_path(const char *input) {
    const char *extension = strrchr(input, '.');
    size_t length = extension ? (size_t)(extension - input) : strlen(input);
    char *output = malloc(length + 3);
    if (!output) return NULL;
    memcpy(output, input, length);
    memcpy(output + length, ".c", 3);
    return output;
}

static char *header_output_path(const char *input) {
    const char *extension = strrchr(input, '.');
    size_t length = extension ? (size_t)(extension - input) : strlen(input);
    char *output = malloc(length + 3);
    if (!output) return NULL;
    memcpy(output, input, length);
    memcpy(output + length, ".h", 3);
    return output;
}

static char *duplicate_string(const char *text);

static char *directory_path(const char *path) {
    const char *slash = strrchr(path, '\\');
    const char *forward = strrchr(path, '/');
    size_t length;
    char *directory;
    if (forward && (!slash || forward > slash)) slash = forward;
    if (!slash) return duplicate_string(".");
    length = (size_t)(slash - path);
    directory = malloc(length + 1);
    if (!directory) return NULL;
    memcpy(directory, path, length);
    directory[length] = '\0';
    return directory;
}

static char *duplicate_string(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1);
    return copy;
}

static char *module_path(const char *directory, const char *module, size_t length,
                         const char *extension) {
    size_t directory_length = strlen(directory);
    size_t extension_length = strlen(extension);
    char *path = malloc(directory_length + 1 + length + extension_length + 1);
    if (!path) return NULL;
    memcpy(path, directory, directory_length);
    path[directory_length] = strchr(directory, '/') ? '/'
        : (strchr(directory, '\\') ? '\\' : '/');
    memcpy(path + directory_length + 1, module, length);
    memcpy(path + directory_length + 1 + length, extension, extension_length + 1);
    return path;
}

static int module_is_active(const ModuleBuildState *state, const char *path) {
    size_t index;
    for (index = 0; index < state->active_count; ++index) {
        if (strcmp(state->active[index], path) == 0) return 1;
    }
    return 0;
}

static int module_is_built(const ModuleBuildState *state, const char *path) {
    size_t index;
    for (index = 0; index < state->built_count; ++index) {
        if (strcmp(state->built[index], path) == 0) return 1;
    }
    return 0;
}

static int write_file(const char *path, const char *data, size_t length);

static int remember_generated(ModuleBuildState *state, const char *path) {
    if (state->generated_count >= sizeof(state->generated) / sizeof(state->generated[0])) return 0;
    state->generated[state->generated_count] = duplicate_string(path);
    if (!state->generated[state->generated_count]) return 0;
    ++state->generated_count;
    return 1;
}

static int compile_module_file(const char *input_path, const char *output_path,
                               ModuleBuildState *state) {
    TrazoPreprocessedSource preprocessed = {0};
    TrazoTokenList tokens = {0};
    TrazoTokenList code_tokens = {0};
    TrazoModuleImportList modules = {0};
    TrazoAst ast = {0};
    TrazoGeneratedC generated = {0};
    TrazoGeneratedC generated_header = {0};
    TrazoLexerError lexer_error = {0};
    TrazoPreprocessorError module_error = {0};
    TrazoParserError parser_error = {0};
    TrazoTypecheckError type_error = {0};
    size_t source_length = 0;
    char *source = read_file(input_path, &source_length);
    char *header_path = header_output_path(output_path);
    char *source_directory = directory_path(input_path);
    int success = 0;
    size_t index;
    (void)source_length;
    if (module_is_active(state, input_path)) {
        fprintf(stderr, "Trazo: cyclic module import: %s\n", input_path);
        free(source);
        free(header_path);
        return 0;
    }
    if (!source || !header_path || !source_directory
        || !trazo_preprocess_source(source, &preprocessed, &module_error)
        || !trazo_lex(preprocessed.source, &tokens, &lexer_error)
        || !trazo_collect_modules(&tokens, &modules, &code_tokens, &module_error)
        || !trazo_parse(&code_tokens, &ast, &parser_error)
        || (state->options->strict && !trazo_typecheck(&code_tokens, &ast, &type_error))
        || !trazo_codegen_c(&code_tokens, &ast, &modules, header_path, &generated, &parser_error)
        || !trazo_codegen_h(&code_tokens, &ast, header_path, &generated_header, &parser_error)) {
        fprintf(stderr, "Trazo: cannot compile module %s\n", input_path);
        goto cleanup;
    }
    state->active[state->active_count++] = duplicate_string(input_path);
    for (index = 0; index < modules.count; ++index) {
        TrazoModuleImport *module = &modules.items[index];
        char *dependency;
        char *dependency_output;
        if (module->kind != TRAZO_MODULE_SOURCE) continue;
        dependency = module_path(source_directory, module->module,
                                 module->module_length, ".trz");
        dependency_output = module_path(state->output_directory, module->module,
                                        module->module_length, ".c");
        if (!dependency || !dependency_output
            || (!module_is_built(state, dependency)
                && !compile_module_file(dependency, dependency_output, state))) {
            free(dependency);
            free(dependency_output);
            goto cleanup;
        }
        free(dependency);
        free(dependency_output);
    }
    if (!write_file(output_path, generated.source, generated.length)
        || !write_file(header_path, generated_header.source, generated_header.length)) goto cleanup;
    if (!remember_generated(state, output_path)) goto cleanup;
    if (state->built_count < sizeof(state->built) / sizeof(state->built[0])) {
        state->built[state->built_count++] = duplicate_string(input_path);
    }
    success = 1;
cleanup:
    if (state->active_count > 0 && state->active[state->active_count - 1]
        && strcmp(state->active[state->active_count - 1], input_path) == 0) {
        free(state->active[--state->active_count]);
    }
    free(header_path);
    free(source_directory);
    trazo_generated_c_free(&generated_header);
    trazo_generated_c_free(&generated);
    trazo_ast_free(&ast);
    trazo_modules_free(&modules);
    trazo_tokens_free(&code_tokens);
    trazo_tokens_free(&tokens);
    trazo_preprocessed_source_free(&preprocessed);
    free(source);
    return success;
}

static int write_file(const char *path, const char *data, size_t length) {
    FILE *file = fopen(path, "wb");
    int success;
    if (!file) return 0;
    success = fwrite(data, 1, length, file) == length && fclose(file) == 0;
    if (!success) fclose(file);
    return success;
}

static int append_command_text(char *command, size_t capacity, size_t *length,
                               const char *text) {
    size_t text_length = strlen(text);
    if (*length + text_length + 1 > capacity) return 0;
    memcpy(command + *length, text, text_length);
    *length += text_length;
    command[*length] = '\0';
    return 1;
}

static int append_command_arg(char *command, size_t capacity, size_t *length,
                              const char *argument) {
    int needs_quotes = strchr(argument, ' ') != NULL || strchr(argument, '\t') != NULL;
    if (!append_command_text(command, capacity, length, needs_quotes ? " \"" : " ")) return 0;
    if (!append_command_text(command, capacity, length, argument)) return 0;
    return needs_quotes ? append_command_text(command, capacity, length, "\"") : 1;
}

static char *detect_vcvars64_bat(void) {
#ifdef _WIN32
    static const char *candidate_roots[] = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio",
        "C:\\Program Files\\Microsoft Visual Studio",
        NULL
    };
    size_t root_index;
    char command[1024];
    char installation[512] = {0};
    char *vcvars_path = NULL;
    FILE *pipe;

    for (root_index = 0; candidate_roots[root_index] != NULL; ++root_index) {
        const char *root = candidate_roots[root_index];
        (void)snprintf(command, sizeof(command),
                       "\"%s\\Installer\\vswhere.exe\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath",
                       root);
        pipe = _popen(command, "r");
        if (!pipe) continue;
        if (!fgets(installation, sizeof(installation), pipe)) {
            _pclose(pipe);
            installation[0] = '\0';
            continue;
        }
        _pclose(pipe);

        installation[strcspn(installation, "\r\n")] = '\0';
        if (installation[0] == '\0') continue;

        vcvars_path = malloc(strlen(installation) + strlen("\\VC\\Auxiliary\\Build\\vcvars64.bat") + 1);
        if (!vcvars_path) return NULL;
        (void)snprintf(vcvars_path,
                       strlen(installation) + strlen("\\VC\\Auxiliary\\Build\\vcvars64.bat") + 1,
                       "%s\\VC\\Auxiliary\\Build\\vcvars64.bat",
                       installation);
        if (access(vcvars_path, F_OK) == 0) return vcvars_path;
        free(vcvars_path);
        vcvars_path = NULL;
    }

    for (root_index = 0; candidate_roots[root_index] != NULL; ++root_index) {
        const char *root = candidate_roots[root_index];
        (void)snprintf(command, sizeof(command),
                       "where /R \"%s\" vcvars64.bat 2>nul",
                       root);
        pipe = _popen(command, "r");
        if (!pipe) continue;
        if (!fgets(installation, sizeof(installation), pipe)) {
            _pclose(pipe);
            installation[0] = '\0';
            continue;
        }
        _pclose(pipe);

        installation[strcspn(installation, "\r\n")] = '\0';
        if (installation[0] == '\0') continue;

        vcvars_path = duplicate_string(installation);
        if (vcvars_path && access(vcvars_path, F_OK) == 0) return vcvars_path;
        free(vcvars_path);
        vcvars_path = NULL;
    }

    return NULL;
#else
    (void)0;
    return NULL;
#endif
}

static int compiler_available(const char *compiler) {
#ifdef _WIN32
    char command[128];
    char *vcvars_path = NULL;
    if (strcmp(compiler, "cl") == 0 || strcmp(compiler, "msvc") == 0) {
        (void)snprintf(command, sizeof(command), "where %s >nul 2>&1", compiler);
        if (system(command) == 0) return 1;
        vcvars_path = detect_vcvars64_bat();
        free(vcvars_path);
        return vcvars_path != NULL;
    }
    (void)snprintf(command, sizeof(command), "where %s >nul 2>&1", compiler);
#else
    char command[128];
    (void)snprintf(command, sizeof(command), "command -v %s >/dev/null 2>&1", compiler);
#endif
    return system(command) == 0;
}

static int append_msvc_tool_invocation(char *command, size_t capacity, size_t *length,
                                       const char *tool) {
    char *vcvars_path = detect_vcvars64_bat();
    if (!vcvars_path) return append_command_arg(command, capacity, length, tool);
    if (!append_command_text(command, capacity, length, "cmd.exe /c \"\"")
        || !append_command_text(command, capacity, length, vcvars_path)
        || !append_command_text(command, capacity, length, "\" && ")
        || !append_command_text(command, capacity, length, tool)) {
        free(vcvars_path);
        return 0;
    }
    free(vcvars_path);
    return 1;
}

static const char *select_compiler(const TrazoOptions *options) {
    if (options->target_compiler) {
        if (strcmp(options->target_compiler, "msvc") == 0) return "cl";
        return options->target_compiler;
    }
#ifdef _WIN32
    if (compiler_available("cl")) return "cl";
    if (compiler_available("clang")) return "clang";
    if (compiler_available("gcc")) return "gcc";
#elif defined(__APPLE__)
    if (compiler_available("clang")) return "clang";
    if (compiler_available("gcc")) return "gcc";
#elif defined(__linux__)
    if (compiler_available("gcc")) return "gcc";
    if (compiler_available("clang")) return "clang";
#else
    if (compiler_available("gcc")) return "gcc";
    if (compiler_available("clang")) return "clang";
#endif
    return NULL;
}

static char *without_extension(const char *path) {
    const char *extension = strrchr(path, '.');
    size_t length = extension ? (size_t)(extension - path) : strlen(path);
    char *result = malloc(length + 1);
    if (!result) return NULL;
    memcpy(result, path, length);
    result[length] = '\0';
    return result;
}

static char *artifact_path_for_kind(const char *path, TrazoOutputKind kind) {
    char *base = without_extension(path);
#ifdef _WIN32
    const char *extension = kind == TRAZO_OUTPUT_SHARED_LIBRARY ? ".dll"
        : kind == TRAZO_OUTPUT_STATIC_LIBRARY ? ".lib" : ".exe";
#else
    const char *extension = kind == TRAZO_OUTPUT_SHARED_LIBRARY ? ".so"
        : kind == TRAZO_OUTPUT_STATIC_LIBRARY ? ".a" : "";
#endif
    size_t base_length;
    size_t extension_length;
    char *result;
    if (!base) return NULL;
    if (extension[0] == '\0') return base;
    base_length = strlen(base);
    extension_length = strlen(extension);
    result = realloc(base, base_length + extension_length + 1);
    if (!result) {
        free(base);
        return NULL;
    }
    memcpy(result + base_length, extension, extension_length + 1);
    return result;
}

static int run_backend(const TrazoOptions *options, const ModuleBuildState *state,
                       const char *artifact) {
    char command[16384] = {0};
    size_t length = 0;
    const char *compiler = select_compiler(options);
    size_t index;
    int is_msvc;

    if (!compiler) {
        fprintf(stderr, "Trazo: no C compiler found\n");
        return 0;
    }

    is_msvc = strcmp(compiler, "cl") == 0 || strcmp(compiler, "msvc") == 0;
    if (options->output_kind == TRAZO_OUTPUT_STATIC_LIBRARY) {
        char *objects[64] = {0};
        size_t object_count = 0;
        for (index = 0; index < state->generated_count; ++index) {
            char *object = without_extension(state->generated[index]);
            size_t object_length;
            if (!object || object_count == sizeof(objects) / sizeof(objects[0])) {
                free(object);
                goto static_fail;
            }
#ifdef _WIN32
            object_length = strlen(object);
            object = realloc(object, object_length + 5);
            if (!object) goto static_fail;
            memcpy(object + object_length, ".obj", 5);
#else
            object_length = strlen(object);
            object = realloc(object, object_length + 3);
            if (!object) goto static_fail;
            memcpy(object + object_length, ".o", 3);
#endif
            objects[object_count++] = object;
            command[0] = '\0';
            length = 0;
            if (is_msvc) {
                if (!append_msvc_tool_invocation(command, sizeof(command), &length, "cl")) goto static_fail;
            } else if (!append_command_arg(command, sizeof(command), &length, compiler)) goto static_fail;
            for (size_t argument_index = 0; argument_index < options->compiler_arg_count; ++argument_index) {
                if (!append_command_arg(command, sizeof(command), &length,
                                        options->compiler_args[argument_index])) goto static_fail;
            }
            if (is_msvc) {
                if (!append_command_text(command, sizeof(command), &length, " /c ")
                    || !append_command_arg(command, sizeof(command), &length, state->generated[index])
                    || !append_command_text(command, sizeof(command), &length, " /Fo:")
                    || !append_command_text(command, sizeof(command), &length, object)) goto static_fail;
            } else if (!append_command_text(command, sizeof(command), &length, " -c")
                || !append_command_arg(command, sizeof(command), &length, state->generated[index])
                || !append_command_text(command, sizeof(command), &length, " -o")
                || !append_command_arg(command, sizeof(command), &length, object)) goto static_fail;
            if (is_msvc) {
                if (!append_command_text(command, sizeof(command), &length, "\"")) goto static_fail;
            }
            printf("Trazo: %s\n", command);
            if (system(command) != 0) goto static_fail;
        }
        command[0] = '\0';
        length = 0;
        if (is_msvc) {
            if (!append_msvc_tool_invocation(command, sizeof(command), &length, "lib")
                || !append_command_text(command, sizeof(command), &length, " /OUT:")) goto static_fail;
            if (!append_command_text(command, sizeof(command), &length, artifact)) goto static_fail;
        } else if (!append_command_text(command, sizeof(command), &length, "ar rcs")
                   || !append_command_arg(command, sizeof(command), &length, artifact)) goto static_fail;
        for (index = 0; index < object_count; ++index) {
            if (!append_command_arg(command, sizeof(command), &length, objects[index])) goto static_fail;
        }
        if (is_msvc) {
            if (!append_command_text(command, sizeof(command), &length, "\"")) goto static_fail;
        }
        printf("Trazo: %s\n", command);
        if (system(command) != 0) goto static_fail;
        for (index = 0; index < object_count; ++index) free(objects[index]);
        return 1;
static_fail:
        for (index = 0; index < sizeof(objects) / sizeof(objects[0]); ++index) free(objects[index]);
        return 0;
    }

    if (is_msvc) {
        if (!append_msvc_tool_invocation(command, sizeof(command), &length, "cl")) return 0;
    } else if (!append_command_arg(command, sizeof(command), &length, compiler)) return 0;
    for (index = 0; index < options->compiler_arg_count; ++index) {
        if (!append_command_arg(command, sizeof(command), &length, options->compiler_args[index])) return 0;
    }
    if (options->output_kind == TRAZO_OUTPUT_SHARED_LIBRARY) {
        if (is_msvc) {
            if (!append_command_text(command, sizeof(command), &length, " /LD")) return 0;
        } else if (!append_command_text(command, sizeof(command), &length, " -shared")) return 0;
    }
    for (index = 0; index < state->generated_count; ++index) {
        if (!append_command_arg(command, sizeof(command), &length, state->generated[index])) return 0;
    }
    if (is_msvc) {
        if (!append_command_text(command, sizeof(command), &length, " /Fe:")
            || !append_command_text(command, sizeof(command), &length, artifact)) return 0;
    } else if (!append_command_text(command, sizeof(command), &length, " -o")
               || !append_command_arg(command, sizeof(command), &length, artifact)) return 0;
    if (is_msvc) {
        if (!append_command_text(command, sizeof(command), &length, "\"")) return 0;
    }
    printf("Trazo: %s\n", command);
    if (system(command) != 0) return 0;
    return 1;
}

static void print_tokens(const TrazoTokenList *tokens) {
    size_t index;
    for (index = 0; index < tokens->count; ++index) {
        const TrazoToken *token = &tokens->items[index];
        printf("%zu:%zu %-16s %.*s\n", token->line, token->column,
               trazo_token_name(token->kind), (int)token->length, token->start);
    }
}

int main(int argc, char **argv) {
    TrazoOptions options = {0};
    TrazoPreprocessedSource preprocessed = {0};
    TrazoTokenList tokens = {0};
    TrazoTokenList code_tokens = {0};
    TrazoModuleImportList modules = {0};
    TrazoAst ast = {0};
    TrazoGeneratedC generated = {0};
    TrazoGeneratedC generated_header = {0};
    TrazoLexerError lexer_error = {0};
    TrazoPreprocessorError module_error = {0};
    TrazoParserError parser_error = {0};
    TrazoTypecheckError type_error = {0};
    char *source = NULL;
    char *output_path = NULL;
    char *header_path = NULL;
    char *output_directory = NULL;
    char *source_directory = NULL;
    char *artifact_path = NULL;
    ModuleBuildState module_state = {0};
    size_t source_length = 0;
    int result = parse_options(argc, argv, &options);

    if (result <= 0) {
        free_compiler_args(&options);
        return result == 0 ? 0 : 1;
    }

    source = read_file(options.input_file, &source_length);
    if (!source) {
        fprintf(stderr, "Trazo: cannot read %s\n", options.input_file);
        free_compiler_args(&options);
        return 1;
    }
    output_path = options.output_file ? default_output_path(options.output_file)
        : default_output_path(options.input_file);
    header_path = output_path ? header_output_path(output_path) : NULL;
    output_directory = output_path ? directory_path(output_path) : NULL;
    source_directory = directory_path(options.input_file);
    if (!trazo_preprocess_source(source, &preprocessed, &module_error)) {
        fprintf(stderr, "Trazo: %zu:%zu: %s\n", module_error.line, module_error.column,
                module_error.message ? module_error.message : "preprocessing failed");
        free(source);
        free_compiler_args(&options);
        return 1;
    }
    if (!trazo_lex(preprocessed.source, &tokens, &lexer_error)) {
        fprintf(stderr, "Trazo: %zu:%zu: %s\n", lexer_error.line, lexer_error.column,
                lexer_error.message);
        goto cleanup_fail;
    }
    if (options.show_tokens) print_tokens(&tokens);
    if (!trazo_collect_modules(&tokens, &modules, &code_tokens, &module_error)
        || !output_directory
        || !source_directory
        || !(module_state.options = &options)
        || !(module_state.output_directory = output_directory)) {
        fprintf(stderr, "Trazo: unable to initialize module build\n");
        goto cleanup_fail;
    }
    module_state.active[0] = duplicate_string(options.input_file);
    if (!module_state.active[0]) goto cleanup_fail;
    module_state.active_count = 1;
    for (size_t module_index = 0; module_index < modules.count; ++module_index) {
        TrazoModuleImport *module = &modules.items[module_index];
        char *dependency;
        char *dependency_output;
        if (module->kind != TRAZO_MODULE_SOURCE) continue;
        dependency = module_path(source_directory, module->module,
                                 module->module_length, ".trz");
        dependency_output = module_path(output_directory, module->module,
                                        module->module_length, ".c");
        if (!dependency || !dependency_output
            || !compile_module_file(dependency, dependency_output, &module_state)) {
            free(dependency);
            free(dependency_output);
            goto cleanup_fail;
        }
        free(dependency);
        free(dependency_output);
    }
    if (!trazo_parse(&code_tokens, &ast, &parser_error)
        || (options.strict && !trazo_typecheck(&code_tokens, &ast, &type_error))
        || !trazo_codegen_c(&code_tokens, &ast, &modules, header_path, &generated, &parser_error)
        || !trazo_codegen_h(&code_tokens, &ast, header_path, &generated_header, &parser_error)) {
        const char *message = parser_error.message ? parser_error.message
            : (type_error.message ? type_error.message : module_error.message);
        size_t line = parser_error.message ? parser_error.line
            : (type_error.message ? type_error.line : module_error.line);
        size_t column = parser_error.message ? parser_error.column
            : (type_error.message ? type_error.column : module_error.column);
        fprintf(stderr, "Trazo: %zu:%zu: %s\n", line, column,
                message ? message : "compilation failed");
        goto cleanup_fail;
    }
    if (!output_path || !header_path || !write_file(output_path, generated.source, generated.length)
        || !write_file(header_path, generated_header.source, generated_header.length)) {
        fprintf(stderr, "Trazo: cannot write generated C file\n");
        goto cleanup_fail;
    }
    if (!remember_generated(&module_state, output_path)) {
        fprintf(stderr, "Trazo: too many generated modules\n");
        goto cleanup_fail;
    }
    printf("Trazo: generated %s\n", output_path);
    printf("Trazo: generated %s\n", header_path);
    if (!options.c_only) {
        artifact_path = artifact_path_for_kind(options.output_file
            ? options.output_file : output_path, options.output_kind);
        if (!artifact_path || !run_backend(&options, &module_state, artifact_path)) {
            fprintf(stderr, "Trazo: C backend failed\n");
            goto cleanup_fail;
        }
        printf("Trazo: generated %s\n", artifact_path);
    }
    free(header_path);
    free(output_path);
    free(output_directory);
    free(source_directory);
    free(artifact_path);
    for (size_t module_index = 0; module_index < module_state.built_count; ++module_index) {
        free(module_state.built[module_index]);
    }
    for (size_t module_index = 0; module_index < module_state.active_count; ++module_index) {
        free(module_state.active[module_index]);
    }
    for (size_t module_index = 0; module_index < module_state.generated_count; ++module_index) {
        free(module_state.generated[module_index]);
    }
    trazo_generated_c_free(&generated);
    trazo_generated_c_free(&generated_header);
    trazo_ast_free(&ast);
    trazo_modules_free(&modules);
    trazo_tokens_free(&code_tokens);
    trazo_tokens_free(&tokens);
    trazo_preprocessed_source_free(&preprocessed);
    free(source);
    free_compiler_args(&options);
    return 0;

cleanup_fail:
    free(output_path);
    free(header_path);
    free(output_directory);
    free(source_directory);
    free(artifact_path);
    for (size_t module_index = 0; module_index < module_state.built_count; ++module_index) {
        free(module_state.built[module_index]);
    }
    for (size_t module_index = 0; module_index < module_state.active_count; ++module_index) {
        free(module_state.active[module_index]);
    }
    for (size_t module_index = 0; module_index < module_state.generated_count; ++module_index) {
        free(module_state.generated[module_index]);
    }
    trazo_generated_c_free(&generated);
    trazo_generated_c_free(&generated_header);
    trazo_ast_free(&ast);
    trazo_modules_free(&modules);
    trazo_tokens_free(&code_tokens);
    trazo_tokens_free(&tokens);
    trazo_preprocessed_source_free(&preprocessed);
    free(source);
    free_compiler_args(&options);
    return 1;
}
