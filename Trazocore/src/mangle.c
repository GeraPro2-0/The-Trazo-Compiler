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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char type_code(TrazoTokenKind kind) {
    if (kind == TRAZO_TOKEN_KW_INT) return 'i';
    if (kind == TRAZO_TOKEN_KW_FLOAT) return 'f';
    if (kind == TRAZO_TOKEN_KW_CHAR) return 'c';
    return 'v';
}

static int is_main_function(const TrazoToken *name) {
    return name->length == 4 && memcmp(name->start, "main", 4) == 0;
}

char *trazo_mangle_function(const TrazoTokenList *tokens, const TrazoAst *ast,
                            size_t function) {
    size_t name = ast->children[ast->first_child[function]];
    size_t params = ast->children[ast->first_child[function] + 2];
    const TrazoToken *name_token = &tokens->items[ast->token_index[name]];
    size_t length = is_main_function(name_token) ? name_token->length : 15 + name_token->length;
    size_t index;
    char *result;

    if (is_main_function(name_token) || (ast->flags[function] & TRAZO_AST_FLAG_EXTERN_C)) {
        length = name_token->length;
    } else {
        for (index = 0; index < ast->child_count[params]; ++index) {
            length += 1;
        }
    }
    result = malloc(length + 1);
    if (!result) return NULL;
    if (is_main_function(name_token)
        || (ast->flags[function] & TRAZO_AST_FLAG_EXTERN_C)) {
        memcpy(result, name_token->start, name_token->length);
        result[length] = '\0';
        return result;
    }
    if (snprintf(result, length + 1, "trazo_global_%.*s__", (int)name_token->length,
                 name_token->start) < 0) {
        free(result);
        return NULL;
    }
    length = strlen(result);
    for (index = 0; index < ast->child_count[params]; ++index) {
        size_t parameter = ast->children[ast->first_child[params] + index];
        size_t parameter_type = ast->children[ast->first_child[parameter]];
        const TrazoToken *type = &tokens->items[ast->token_index[parameter_type]];
        result[length++] = type_code(type->kind);
    }
    result[length] = '\0';
    return result;
}
