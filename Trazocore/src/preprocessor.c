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

#include "trazo.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    size_t length;
} Define;

typedef struct {
    int parent_active;
    int active;
    int branch_taken;
} Conditional;

static int name_equal(const char *left, size_t left_length,
                      const char *right, size_t right_length) {
    return left_length == right_length && memcmp(left, right, left_length) == 0;
}

static int is_defined(const Define *defines, size_t count,
                      const char *name, size_t length) {
    size_t index;
    for (index = 0; index < count; ++index) {
        if (name_equal(defines[index].name, defines[index].length, name, length)) return 1;
    }
    return 0;
}

static void clear_line(char *destination, size_t length) {
    memset(destination, ' ', length);
}

static const char *skip_spaces(const char *cursor, const char *end) {
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    return cursor;
}

static const char *read_word(const char *cursor, const char *end, size_t *length) {
    const char *start = cursor;
    while (cursor < end && ((*cursor >= 'a' && *cursor <= 'z')
            || (*cursor >= 'A' && *cursor <= 'Z')
            || (*cursor >= '0' && *cursor <= '9') || *cursor == '_')) ++cursor;
    *length = (size_t)(cursor - start);
    return cursor;
}

static int append_define(Define **defines, size_t *count, const char *name, size_t length) {
    Define *items = realloc(*defines, (*count + 1) * sizeof(**defines));
    if (!items) return 0;
    items[*count].name = malloc(length + 1);
    if (!items[*count].name) {
        free(items);
        return 0;
    }
    memcpy(items[*count].name, name, length);
    items[*count].name[length] = '\0';
    items[*count].length = length;
    *defines = items;
    ++*count;
    return 1;
}

static void remove_define(Define *defines, size_t *count, const char *name, size_t length) {
    size_t index;
    for (index = 0; index < *count; ++index) {
        if (name_equal(defines[index].name, defines[index].length, name, length)) {
            free(defines[index].name);
            defines[index] = defines[--*count];
            return;
        }
    }
}

static void free_defines(Define *defines, size_t count) {
    while (count > 0) free(defines[--count].name);
    free(defines);
}

static void set_error(TrazoPreprocessorError *error, const char *message,
                      const TrazoToken *token) {
    if (error && !error->message) {
        error->message = message;
        error->line = token->line;
        error->column = token->column;
    }
}

static int append_import(TrazoCImportList *imports, const TrazoToken *header) {
    TrazoCImport *items;
    size_t capacity;

    if (imports->count == imports->capacity) {
        capacity = imports->capacity == 0 ? 8 : imports->capacity * 2;
        items = realloc(imports->items, capacity * sizeof(*items));
        if (!items) return 0;
        imports->items = items;
        imports->capacity = capacity;
    }
    imports->items[imports->count++] = (TrazoCImport){
        header->start, header->length, header->line, header->column
    };
    return 1;
}

int trazo_preprocess(const TrazoTokenList *tokens, TrazoCImportList *imports,
                     TrazoPreprocessorError *error) {
    size_t index;

    if (!tokens || !imports || tokens->count == 0) return 0;
    *imports = (TrazoCImportList){0};
    if (error) *error = (TrazoPreprocessorError){0};

    for (index = 0; index < tokens->count; ++index) {
        const TrazoToken *token = &tokens->items[index];
        const TrazoToken *header;
        const TrazoToken *last;

        if (token->kind != TRAZO_TOKEN_KW_IMPORT_C) continue;
        if (index + 2 >= tokens->count
            || tokens->items[index + 1].kind != TRAZO_TOKEN_LESS) {
            set_error(error, "expected '<' after importC", token);
            goto fail;
        }
        header = &tokens->items[index + 2];
        if (header->kind != TRAZO_TOKEN_IDENTIFIER
            && header->kind != TRAZO_TOKEN_STRING) {
            set_error(error, "expected C header name", header);
            goto fail;
        }
        last = header;
        while (tokens->items[index + 3].kind != TRAZO_TOKEN_GREATER
               && tokens->items[index + 3].kind != TRAZO_TOKEN_EOF) {
            last = &tokens->items[index + 3];
            ++index;
        }
        if (tokens->items[index + 3].kind != TRAZO_TOKEN_GREATER) {
            set_error(error, "expected '>' after C header name", &tokens->items[index + 3]);
            goto fail;
        }
        if (header->kind == TRAZO_TOKEN_STRING) {
            last = header;
        }
        header = &(TrazoToken){
            header->kind,
            header->start,
            (size_t)((last->start + last->length) - header->start),
            header->line,
            header->column
        };
        if (!append_import(imports, header)) {
            set_error(error, "out of memory", token);
            goto fail;
        }
        ++index;
    }
    return 1;

fail:
    trazo_c_imports_free(imports);
    return 0;
}

void trazo_c_imports_free(TrazoCImportList *imports) {
    if (imports) {
        free(imports->items);
        *imports = (TrazoCImportList){0};
    }
}

int trazo_preprocess_source(const char *source, TrazoPreprocessedSource *output,
                            TrazoPreprocessorError *error) {
    Define *defines = NULL;
    Conditional conditionals[64];
    size_t define_count = 0;
    size_t conditional_count = 0;
    size_t length;
    size_t line = 1;
    char *filtered;
    const char *cursor;

    if (!source || !output) return 0;
    *output = (TrazoPreprocessedSource){0};
    if (error) *error = (TrazoPreprocessorError){0};
    length = strlen(source);
    filtered = malloc(length + 1);
    if (!filtered) return 0;
    memcpy(filtered, source, length + 1);
    cursor = source;

    while (*cursor) {
        const char *line_start = cursor;
        const char *line_end = strchr(cursor, '\n');
        const char *directive;
        const char *directive_start;
        const char *argument;
        const char *argument_start;
        size_t directive_length;
        size_t argument_length;
        int active = conditional_count == 0 || conditionals[conditional_count - 1].active;
        int is_directive;

        if (!line_end) line_end = source + length;
        directive = skip_spaces(line_start, line_end);
        is_directive = directive < line_end && *directive == '#';
        if (is_directive) {
            ++directive;
            directive = skip_spaces(directive, line_end);
            directive_start = directive;
            directive = read_word(directive, line_end, &directive_length);
            argument = skip_spaces(directive, line_end);
            argument_start = argument;
            (void)read_word(argument, line_end, &argument_length);

            if (name_equal(directive_start, directive_length, "if", 2)) {
                if (conditional_count == 64) {
                    if (error) *error = (TrazoPreprocessorError){"invalid conditional directive", line, 1};
                    goto fail;
                }
                conditionals[conditional_count++] = (Conditional){active, active, active};
            } else if (name_equal(directive_start, directive_length, "ifdef", 5)
                || name_equal(directive_start, directive_length, "ifndef", 6)) {
                int exists;
                if (argument_length == 0 || conditional_count == 64) {
                    if (error) *error = (TrazoPreprocessorError){"invalid conditional directive", line, 1};
                    goto fail;
                }
                exists = is_defined(defines, define_count, argument_start, argument_length);
                conditionals[conditional_count++] = (Conditional){
                    active, active && (name_equal(directive_start, directive_length, "ifdef", 5) ? exists : !exists), 0
                };
                conditionals[conditional_count - 1].branch_taken = conditionals[conditional_count - 1].active;
            } else if (name_equal(directive_start, directive_length, "else", 4)) {
                Conditional *conditional;
                if (conditional_count == 0 || argument_length != 0) {
                    if (error) *error = (TrazoPreprocessorError){"invalid #else directive", line, 1};
                    goto fail;
                }
                conditional = &conditionals[conditional_count - 1];
                conditional->active = conditional->parent_active && !conditional->branch_taken;
                conditional->branch_taken = 1;
            } else if (name_equal(directive_start, directive_length, "elif", 4)) {
                Conditional *conditional;
                int exists;
                if (conditional_count == 0 || argument_length == 0) {
                    if (error) *error = (TrazoPreprocessorError){"invalid #elif directive", line, 1};
                    goto fail;
                }
                conditional = &conditionals[conditional_count - 1];
                exists = is_defined(defines, define_count, argument_start, argument_length);
                conditional->active = conditional->parent_active && !conditional->branch_taken && exists;
                if (conditional->active) conditional->branch_taken = 1;
            } else if (name_equal(directive_start, directive_length, "endif", 5)) {
                if (conditional_count == 0 || argument_length != 0) {
                    if (error) *error = (TrazoPreprocessorError){"invalid #endif directive", line, 1};
                    goto fail;
                }
                --conditional_count;
            } else if (name_equal(directive_start, directive_length, "define", 6)) {
                if (active && argument_length > 0) {
                    remove_define(defines, &define_count, argument_start, argument_length);
                    if (!append_define(&defines, &define_count, argument_start, argument_length)) goto fail;
                }
            } else if (name_equal(directive_start, directive_length, "undef", 5)) {
                if (active && argument_length > 0) remove_define(defines, &define_count, argument_start, argument_length);
            } else if (name_equal(directive_start, directive_length, "include", 7)) {
                size_t line_length = (size_t)(line_end - line_start);
                size_t argument_offset = (size_t)(argument - line_start);
                clear_line(filtered + (line_start - source), line_length);
                if (active && argument < line_end && line_length >= 8
                    && line_length - argument_offset + 8 <= line_length) {
                    memcpy(filtered + (line_start - source), "importC ", 8);
                    memcpy(filtered + (line_start - source) + 8, argument,
                           line_length - argument_offset);
                }
            } else {
                /* Bootstrap mode leaves active C directives to the host compiler. */
            }
            if (!name_equal(directive_start, directive_length, "include", 7)) {
                clear_line(filtered + (line_start - source), (size_t)(line_end - line_start));
            }
        } else if (!active) {
            clear_line(filtered + (line_start - source), (size_t)(line_end - line_start));
        }
        cursor = *line_end ? line_end + 1 : line_end;
        if (*line_end) ++line;
    }
    if (conditional_count != 0) {
        if (error) *error = (TrazoPreprocessorError){"unterminated conditional directive", line, 1};
        goto fail;
    }
    filtered[length] = '\0';
    output->source = filtered;
    output->length = length;
    free_defines(defines, define_count);
    return 1;

fail:
    free(filtered);
    free_defines(defines, define_count);
    return 0;
}

void trazo_preprocessed_source_free(TrazoPreprocessedSource *output) {
    if (output) {
        free(output->source);
        *output = (TrazoPreprocessedSource){0};
    }
}