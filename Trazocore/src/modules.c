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

static void fail(TrazoPreprocessorError *error, const char *message, const TrazoToken *token) {
    if (error && !error->message) {
        error->message = message;
        error->line = token->line;
        error->column = token->column;
    }
}

static int append_module(TrazoModuleImportList *modules, TrazoModuleImport module) {
    TrazoModuleImport *items;
    size_t capacity;
    if (modules->count == modules->capacity) {
        capacity = modules->capacity == 0 ? 8 : modules->capacity * 2;
        items = realloc(modules->items, capacity * sizeof(*items));
        if (!items) return 0;
        modules->items = items;
        modules->capacity = capacity;
    }
    modules->items[modules->count++] = module;
    return 1;
}

static int append_code_token(TrazoTokenList *tokens, TrazoToken token) {
    TrazoToken *items;
    size_t capacity;
    if (tokens->count == tokens->capacity) {
        capacity = tokens->capacity == 0 ? 64 : tokens->capacity * 2;
        items = realloc(tokens->items, capacity * sizeof(*items));
        if (!items) return 0;
        tokens->items = items;
        tokens->capacity = capacity;
    }
    tokens->items[tokens->count++] = token;
    return 1;
}

static int is_name(TrazoTokenKind kind) {
    return kind == TRAZO_TOKEN_IDENTIFIER;
}

int trazo_collect_modules(const TrazoTokenList *tokens, TrazoModuleImportList *modules,
                          TrazoTokenList *code_tokens, TrazoPreprocessorError *error) {
    size_t index = 0;
    if (!tokens || !modules || !code_tokens || tokens->count == 0) return 0;
    *modules = (TrazoModuleImportList){0};
    *code_tokens = (TrazoTokenList){0};
    if (error) *error = (TrazoPreprocessorError){0};

    while (index < tokens->count) {
        const TrazoToken *token = &tokens->items[index];
        TrazoModuleImport module = {0};
        size_t end;
        if (token->kind == TRAZO_TOKEN_KW_IMPORT_C) {
            if (!trazo_collect_ffi_import(tokens, index, modules, error, &end)) goto fail;
            index = end;
            continue;
        } else if (token->kind == TRAZO_TOKEN_KW_IMPORT) {
            if (index + 1 >= tokens->count || !is_name(tokens->items[index + 1].kind)) {
                fail(error, "expected Trazo module name", token); goto fail;
            }
            end = index + 1;
            module.kind = TRAZO_MODULE_SOURCE;
            module.module = tokens->items[end].start;
            module.module_length = tokens->items[end].length;
            if (end + 2 < tokens->count && tokens->items[end + 1].kind == TRAZO_TOKEN_KW_AS) {
                if (!is_name(tokens->items[end + 2].kind)) { fail(error, "expected module alias", &tokens->items[end + 2]); goto fail; }
                end += 2;
                module.alias = tokens->items[end].start;
                module.alias_length = tokens->items[end].length;
            }
        } else if (token->kind == TRAZO_TOKEN_KW_FROM) {
            if (index + 3 >= tokens->count || !is_name(tokens->items[index + 1].kind)
                || tokens->items[index + 2].kind != TRAZO_TOKEN_KW_IMPORT
                || !is_name(tokens->items[index + 3].kind)) {
                fail(error, "expected from module import symbol", token); goto fail;
            }
            end = index + 3;
            module.kind = TRAZO_MODULE_SOURCE;
            module.module = tokens->items[index + 1].start;
            module.module_length = tokens->items[index + 1].length;
            module.symbol = tokens->items[index + 3].start;
            module.symbol_length = tokens->items[index + 3].length;
            if (end + 2 < tokens->count && tokens->items[end + 1].kind == TRAZO_TOKEN_KW_AS) {
                if (!is_name(tokens->items[end + 2].kind)) { fail(error, "expected imported symbol alias", &tokens->items[end + 2]); goto fail; }
                end += 2;
                module.alias = tokens->items[end].start;
                module.alias_length = tokens->items[end].length;
            }
        } else {
            if (!append_code_token(code_tokens, *token)) { fail(error, "out of memory", token); goto fail; }
            ++index;
            continue;
        }
        module.line = token->line;
        module.column = token->column;
        if (!append_module(modules, module)) { fail(error, "out of memory", token); goto fail; }
        index = end + 1;
    }
    return 1;
fail:
    trazo_modules_free(modules);
    trazo_tokens_free(code_tokens);
    return 0;
}

void trazo_modules_free(TrazoModuleImportList *modules) {
    if (modules) {
        free(modules->items);
        *modules = (TrazoModuleImportList){0};
    }
}