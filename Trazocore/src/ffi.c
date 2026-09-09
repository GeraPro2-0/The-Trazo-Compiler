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

int trazo_collect_ffi_import(const TrazoTokenList *tokens, size_t index,
                            TrazoModuleImportList *modules,
                            TrazoPreprocessorError *error, size_t *next_index) {
    const TrazoToken *token = &tokens->items[index];
    TrazoModuleImport module = {0};
    size_t end;
    size_t header_start;

    if (index + 2 >= tokens->count) {
        fail(error, "expected C header after importC", token);
        return 0;
    }
    if (tokens->items[index + 1].kind == TRAZO_TOKEN_STRING) {
        module.kind = TRAZO_MODULE_C_SOURCE;
        module.module = tokens->items[index + 1].start;
        module.module_length = tokens->items[index + 1].length;
        end = index + 1;
    } else {
        if (tokens->items[index + 1].kind != TRAZO_TOKEN_LESS) {
            fail(error, "expected '<' or C source string after importC", token);
            return 0;
        }
        header_start = index + 2;
        end = header_start;
        while (end < tokens->count && tokens->items[end].kind != TRAZO_TOKEN_GREATER) ++end;
        if (end >= tokens->count) {
            fail(error, "expected '>' after C header", token);
            return 0;
        }
        module.kind = TRAZO_MODULE_C_HEADER;
        module.module = tokens->items[header_start].start;
        module.module_length = (size_t)((tokens->items[end - 1].start + tokens->items[end - 1].length) - module.module);
    }

    module.line = token->line;
    module.column = token->column;
    if (!append_module(modules, module)) {
        fail(error, "out of memory", token);
        return 0;
    }

    if (next_index) *next_index = end + 1;
    return 1;
}
