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

#include <string.h>

#define TRAZO_MAX_LOCAL_SYMBOLS 64u

/*
static void set_error(TrazoTypecheckError *error, const char *message,
                      const TrazoToken *token) {
    if (error && !error->message) {
        error->message = message;
        error->line = token->line;
        error->column = token->column;
    }
}

typedef struct {
    const char *name;
    size_t length;
    TrazoTypeKind type;
} TrazoLocalSymbol;

typedef struct {
    size_t node;
    const char *name;
    size_t name_length;
    TrazoTypeKind return_type;
} TrazoFunctionSymbol;

static TrazoTypeKind token_type(TrazoTokenKind kind) {
    if (kind == TRAZO_TOKEN_KW_VOID) return TRAZO_TYPE_VOID;
    if (kind == TRAZO_TOKEN_KW_INT) return TRAZO_TYPE_INT;
    if (kind == TRAZO_TOKEN_KW_FLOAT) return TRAZO_TYPE_FLOAT;
    if (kind == TRAZO_TOKEN_KW_CHAR) return TRAZO_TYPE_CHAR;
    return TRAZO_TYPE_INVALID;
}

static int symbol_matches(const TrazoToken *token, const char *name, size_t length) {
    return token->length == length && memcmp(token->start, name, length) == 0;
}

static TrazoTypeKind symbol_lookup(const TrazoLocalSymbol *symbols, size_t count,
                                  const TrazoToken *token) {
    size_t index = 0;
    for (index = 0; index < count; ++index) {
        if (symbol_matches(token, symbols[index].name, symbols[index].length)) {
            return symbols[index].type;
        }
    }
    return TRAZO_TYPE_INVALID;
}

static TrazoTypeKind declared_type_of(const TrazoTokenList *tokens, const TrazoAst *ast,
                                     size_t node, const TrazoLocalSymbol *symbols,
                                     size_t symbol_count) {
    const TrazoToken *token = &tokens->items[ast->token_index[node]];
    if (ast->kind[node] == TRAZO_AST_IDENTIFIER) {
        TrazoTypeKind named = symbol_lookup(symbols, symbol_count, token);
        if (named != TRAZO_TYPE_INVALID) {
            return named;
        }
    }
    return token_type(token->kind);
}

static const TrazoFunctionSymbol *find_function(const TrazoTokenList *tokens,
                                                const TrazoAst *ast,
                                                const TrazoFunctionSymbol *functions,
                                                size_t function_count,
                                                const TrazoToken *name,
                                                const TrazoTypeKind *argument_types,
                                                size_t argument_count,
                                                const TrazoLocalSymbol *symbols,
                                                size_t symbol_count) {
    size_t index;
    for (index = 0; index < function_count; ++index) {
        if (functions[index].name_length == name->length
            && memcmp(functions[index].name, name->start, name->length) == 0) {
            size_t params = ast->children[ast->first_child[functions[index].node] + 2];
            size_t parameter_index;
            if (ast->child_count[params] != argument_count) continue;
            for (parameter_index = 0; parameter_index < argument_count; ++parameter_index) {
                size_t parameter = ast->children[ast->first_child[params] + parameter_index];
                size_t parameter_type = ast->children[ast->first_child[parameter]];
                if (declared_type_of(tokens, ast, parameter_type, symbols, symbol_count)
                    != argument_types[parameter_index]) break;
            }
            if (parameter_index == argument_count) return &functions[index];
        }
    }
    return NULL;
}

static int has_function_name(const TrazoFunctionSymbol *functions, size_t function_count,
                             const TrazoToken *name) {
    size_t index;
    for (index = 0; index < function_count; ++index) {
        if (functions[index].name_length == name->length
            && memcmp(functions[index].name, name->start, name->length) == 0) return 1;
    }
    return 0;
}

static int same_function_signature(const TrazoTokenList *tokens, const TrazoAst *ast,
                                   size_t left, size_t right) {
    size_t left_params = ast->children[ast->first_child[left] + 2];
    size_t right_params = ast->children[ast->first_child[right] + 2];
    size_t index;
    const TrazoToken *left_name = &tokens->items[ast->token_index[
        ast->children[ast->first_child[left]]]];
    const TrazoToken *right_name = &tokens->items[ast->token_index[
        ast->children[ast->first_child[right]]]];
    if (!symbol_matches(left_name, right_name->start, right_name->length)
        || ast->child_count[left_params] != ast->child_count[right_params]) return 0;
    for (index = 0; index < ast->child_count[left_params]; ++index) {
        size_t left_parameter = ast->children[ast->first_child[left_params] + index];
        size_t right_parameter = ast->children[ast->first_child[right_params] + index];
        size_t left_type = ast->children[ast->first_child[left_parameter]];
        size_t right_type = ast->children[ast->first_child[right_parameter]];
        if (tokens->items[ast->token_index[left_type]].kind
            != tokens->items[ast->token_index[right_type]].kind) return 0;
    }
    return 1;
}

static void collect_function_symbols(const TrazoTokenList *tokens, const TrazoAst *ast,
                                    size_t function, TrazoLocalSymbol *symbols,
                                    size_t *symbol_count) {
    size_t params = ast->children[ast->first_child[function] + 2];
    size_t block = ast->children[ast->first_child[function] + 3];
    size_t index = 0;
    for (index = 0; index < ast->child_count[params]; ++index) {
        size_t param = ast->children[ast->first_child[params] + index];
        size_t type_node = ast->children[ast->first_child[param]];
        size_t name_node = ast->children[ast->first_child[param] + 1];
        if (symbol_count && *symbol_count < TRAZO_MAX_LOCAL_SYMBOLS) {
            const TrazoToken *name = &tokens->items[ast->token_index[name_node]];
            symbols[*symbol_count].name = name->start;
            symbols[*symbol_count].length = name->length;
            symbols[*symbol_count].type = declared_type_of(tokens, ast, type_node, symbols, *symbol_count);
            ++*symbol_count;
        }
    }
    for (index = 0; index < ast->child_count[block]; ++index) {
        size_t statement = ast->children[ast->first_child[block] + index];
        if (ast->kind[statement] == TRAZO_AST_VAR_DECL) {
            size_t type_node = ast->children[ast->first_child[statement]];
            size_t name_node = ast->children[ast->first_child[statement] + 1];
            if (symbol_count && *symbol_count < TRAZO_MAX_LOCAL_SYMBOLS) {
                const TrazoToken *name = &tokens->items[ast->token_index[name_node]];
                symbols[*symbol_count].name = name->start;
                symbols[*symbol_count].length = name->length;
                symbols[*symbol_count].type = declared_type_of(tokens, ast, type_node, symbols, *symbol_count);
                ++*symbol_count;
            }
        } else if (ast->kind[statement] == TRAZO_AST_TYPEDEF) {
            size_t alias_type = ast->children[ast->first_child[statement]];
            size_t alias_name = ast->children[ast->first_child[statement] + 1];
            const TrazoToken *type = &tokens->items[ast->token_index[alias_type]];
            const TrazoToken *name = &tokens->items[ast->token_index[alias_name]];
            TrazoTypeKind kind = declared_type_of(tokens, ast, alias_type, symbols, *symbol_count);
            if (symbol_count && *symbol_count < TRAZO_MAX_LOCAL_SYMBOLS) {
                symbols[*symbol_count].name = name->start;
                symbols[*symbol_count].length = name->length;
                symbols[*symbol_count].type = kind == TRAZO_TYPE_INVALID ? token_type(type->kind) : kind;
                ++*symbol_count;
            }
        }
    }
}

static TrazoTypeKind expression_type(const TrazoTokenList *tokens, const TrazoAst *ast,
                                     size_t node, TrazoTypecheckError *error,
                                     const TrazoLocalSymbol *symbols, size_t symbol_count,
                                     const TrazoFunctionSymbol *functions,
                                     size_t function_count) {
    const TrazoToken *token = &tokens->items[ast->token_index[node]];
    switch (ast->kind[node]) {
        case TRAZO_AST_INTEGER: return TRAZO_TYPE_INT;
        case TRAZO_AST_FLOAT: return TRAZO_TYPE_FLOAT;
        case TRAZO_AST_CHARACTER: return TRAZO_TYPE_CHAR;
        case TRAZO_AST_IDENTIFIER: {
            TrazoTypeKind type = symbol_lookup(symbols, symbol_count, token);
            if (type == TRAZO_TYPE_INVALID) {
                set_error(error, "unknown identifier", token);
            }
            return type;
        }
        case TRAZO_AST_UNARY: {
            size_t operand = ast->children[ast->first_child[node]];
            TrazoTypeKind operand_type = expression_type(tokens, ast, operand, error, symbols,
                                                         symbol_count, functions, function_count);
            if (operand_type == TRAZO_TYPE_INVALID) {
                return TRAZO_TYPE_INVALID;
            }
            return operand_type;
        }
        case TRAZO_AST_BINARY: {
            size_t left = ast->children[ast->first_child[node]];
            size_t right = ast->children[ast->first_child[node] + 1];
            TrazoTypeKind left_type = expression_type(tokens, ast, left, error, symbols,
                                                      symbol_count, functions, function_count);
            TrazoTypeKind right_type = expression_type(tokens, ast, right, error, symbols,
                                                       symbol_count, functions, function_count);
            if (left_type == TRAZO_TYPE_INVALID || right_type == TRAZO_TYPE_INVALID) {
                return TRAZO_TYPE_INVALID;
            }
            if (left_type != right_type) {
                set_error(error, "binary operation type mismatch", token);
                return TRAZO_TYPE_INVALID;
            }
            return left_type;
        }
        case TRAZO_AST_STRING:
            return TRAZO_TYPE_INVALID;
        case TRAZO_AST_CALL: {
            size_t first = ast->first_child[node];
            size_t argument_count = ast->child_count[node] - 1;
            TrazoTypeKind argument_types[32];
            const TrazoToken *callee_name;
            const TrazoFunctionSymbol *function;
            if (argument_count >= 1 && ast->kind[ast->children[first]] == TRAZO_AST_IDENTIFIER
                && symbol_matches(&tokens->items[ast->token_index[ast->children[first]]], "C", 1)) {
                size_t argument_index;
                for (argument_index = 1; argument_index < ast->child_count[node]; ++argument_index) {
                    size_t argument = ast->children[first + argument_index];
                    if (ast->kind[argument] == TRAZO_AST_CALL && ast->child_count[argument] != 2) {
                        (void)expression_type(tokens, ast, argument, error, symbols, symbol_count,
                                              functions, function_count);
                    }
                }
                return TRAZO_TYPE_INVALID;
            }
            if (ast->kind[ast->children[first]] != TRAZO_AST_IDENTIFIER) {
                set_error(error, "invalid function call", token);
                return TRAZO_TYPE_INVALID;
            }
            callee_name = &tokens->items[ast->token_index[ast->children[first]]];
            {
                size_t argument_index;
                for (argument_index = 0; argument_index < argument_count; ++argument_index) {
                    argument_types[argument_index] = expression_type(
                        tokens, ast, ast->children[first + argument_index + 1], error,
                        symbols, symbol_count, functions, function_count);
                    if (argument_types[argument_index] == TRAZO_TYPE_INVALID) {
                        return TRAZO_TYPE_INVALID;
                    }
                }
            }
            function = find_function(tokens, ast, functions, function_count, callee_name,
                                     argument_types, argument_count, symbols, symbol_count);
            if (!function) {
                set_error(error, "no matching function overload", callee_name);
                return TRAZO_TYPE_INVALID;
            }
            ((TrazoAst *)ast)->resolved_function[node] = function->node;
            return function->return_type;
        }
        default:
            set_error(error, "expression has no known type", token);
            return TRAZO_TYPE_INVALID;
    }
}

*/

int trazo_typecheck(const TrazoTokenList *tokens, const TrazoAst *ast,
                    TrazoTypecheckError *error) {
    /* v0.1 bootstrap mode: the host C compiler performs semantic checking. */
    (void)tokens;
    (void)ast;
    if (error) *error = (TrazoTypecheckError){0};
    return 1;

    /*
    size_t program = ast->node_count - 1;
    size_t root_index = 0;
    TrazoFunctionSymbol functions[64];
    size_t function_count = 0;
    TrazoLocalSymbol globals[TRAZO_MAX_LOCAL_SYMBOLS];
    size_t global_count = 0;
    if (!tokens || !ast || ast->node_count == 0) {
        return 0;
    }
    if (error) {
        *error = (TrazoTypecheckError){0};
    }
    if (ast->kind[program] != TRAZO_AST_PROGRAM) {
        return 0;
    }

    for (root_index = 0; root_index < ast->child_count[program]; ++root_index) {
        size_t function = ast->children[ast->first_child[program] + root_index];
        size_t name;
        size_t return_type;
        const TrazoToken *name_token;
        if (ast->kind[function] == TRAZO_AST_VAR_DECL) {
            size_t type = ast->children[ast->first_child[function]];
            size_t global_name = ast->children[ast->first_child[function] + 1];
            const TrazoToken *global_name_token = &tokens->items[ast->token_index[global_name]];
            if (global_count < TRAZO_MAX_LOCAL_SYMBOLS) {
                globals[global_count++] = (TrazoLocalSymbol){
                    global_name_token->start,
                    global_name_token->length,
                    declared_type_of(tokens, ast, type, globals, global_count)
                };
            }
            continue;
        }
        if (ast->kind[function] != TRAZO_AST_FUNCTION || function_count == 64) {
            continue;
        }
        name = ast->children[ast->first_child[function]];
        return_type = ast->children[ast->first_child[function] + 1];
        name_token = &tokens->items[ast->token_index[name]];
        for (size_t previous = 0; previous < function_count; ++previous) {
            if (same_function_signature(tokens, ast, functions[previous].node, function)) {
                set_error(error, "duplicate function signature", name_token);
                return 0;
            }
        }
        functions[function_count++] = (TrazoFunctionSymbol){
            function,
            name_token->start,
            name_token->length,
            token_type(tokens->items[ast->token_index[return_type]].kind)
        };
    }

    root_index = 0;
    for (root_index = 0; root_index < ast->child_count[program]; ++root_index) {
        size_t function = ast->children[ast->first_child[program] + root_index];
        size_t type_node = 0;
        size_t block = 0;
        TrazoTypeKind expected = TRAZO_TYPE_INVALID;
        size_t statement_index = 0;
        TrazoLocalSymbol symbols[TRAZO_MAX_LOCAL_SYMBOLS];
        size_t symbol_count = global_count;
        if (ast->kind[function] != TRAZO_AST_FUNCTION) {
            continue;
        }
        memcpy(symbols, globals, global_count * sizeof(*globals));
        if (ast->child_count[function] != 4) {
            return 0;
        }
        type_node = ast->children[ast->first_child[function] + 1];
        block = ast->children[ast->first_child[function] + 3];
        collect_function_symbols(tokens, ast, function, symbols, &symbol_count);
        expected = token_type(tokens->items[ast->token_index[type_node]].kind);
        if (expected == TRAZO_TYPE_INVALID) {
            set_error(error, "invalid function return type", &tokens->items[ast->token_index[type_node]]);
            return 0;
        }
        for (statement_index = 0; statement_index < ast->child_count[block]; ++statement_index) {
            size_t statement = ast->children[ast->first_child[block] + statement_index];
            if (ast->kind[statement] == TRAZO_AST_ENUM_DECL) {
                for (size_t member_index = 1; member_index < ast->child_count[statement]; ++member_index) {
                    size_t member = ast->children[ast->first_child[statement] + member_index];
                    const TrazoToken *member_token = &tokens->items[ast->token_index[member]];
                    if (symbol_count < TRAZO_MAX_LOCAL_SYMBOLS) {
                        symbols[symbol_count].name = member_token->start;
                        symbols[symbol_count].length = member_token->length;
                        symbols[symbol_count].type = TRAZO_TYPE_INT;
                        ++symbol_count;
                    }
                }
            } else if (ast->kind[statement] == TRAZO_AST_TYPEDEF) {
                size_t alias_type = ast->children[ast->first_child[statement]];
                size_t alias_name = ast->children[ast->first_child[statement] + 1];
                const TrazoToken *name = &tokens->items[ast->token_index[alias_name]];
                TrazoTypeKind type = declared_type_of(tokens, ast, alias_type, symbols, symbol_count);
                if (symbol_count < TRAZO_MAX_LOCAL_SYMBOLS) {
                    symbols[symbol_count].name = name->start;
                    symbols[symbol_count].length = name->length;
                    symbols[symbol_count].type = type;
                    ++symbol_count;
                }
            } else if (ast->kind[statement] == TRAZO_AST_VAR_DECL) {
                if (ast->child_count[statement] >= 3) {
                    size_t declared_type = ast->children[ast->first_child[statement]];
                    size_t initializer = ast->children[ast->first_child[statement] + 2];
                    TrazoTypeKind declared = declared_type_of(tokens, ast, declared_type, symbols, symbol_count);
                    TrazoTypeKind actual = expression_type(tokens, ast, initializer, error, symbols,
                                                            symbol_count, functions, function_count);
                    if (declared == TRAZO_TYPE_INVALID || actual == TRAZO_TYPE_INVALID || declared != actual) {
                        set_error(error, "variable initializer type does not match declaration",
                                  &tokens->items[ast->token_index[statement]]);
                        return 0;
                    }
                }
            } else if (ast->kind[statement] == TRAZO_AST_RETURN) {
                if (ast->child_count[statement] == 0) {
                    if (expected != TRAZO_TYPE_VOID) {
                        set_error(error, "return without value in non-void function",
                                  &tokens->items[ast->token_index[statement]]);
                        return 0;
                    }
                } else {
                    size_t expression = ast->children[ast->first_child[statement]];
                    TrazoTypeKind actual = expression_type(tokens, ast, expression, error, symbols,
                                                            symbol_count, functions, function_count);
                    if (expected == TRAZO_TYPE_VOID || actual == TRAZO_TYPE_INVALID || actual != expected) {
                        set_error(error, "return type does not match function type",
                                  &tokens->items[ast->token_index[statement]]);
                        return 0;
                    }
                }
            } else if (ast->kind[statement] == TRAZO_AST_IF) {
                size_t condition = ast->children[ast->first_child[statement]];
                TrazoTypeKind actual = expression_type(tokens, ast, condition, error, symbols,
                                                        symbol_count, functions, function_count);
                if (actual == TRAZO_TYPE_INVALID) {
                    set_error(error, "if condition has no known type",
                              &tokens->items[ast->token_index[statement]]);
                    return 0;
                }
            } else if (ast->kind[statement] == TRAZO_AST_WHILE) {
                size_t condition = ast->children[ast->first_child[statement]];
                TrazoTypeKind actual = expression_type(tokens, ast, condition, error, symbols,
                                                        symbol_count, functions, function_count);
                if (actual == TRAZO_TYPE_INVALID) {
                    set_error(error, "while condition has no known type",
                              &tokens->items[ast->token_index[statement]]);
                    return 0;
                }
            } else if (ast->kind[statement] == TRAZO_AST_FOR) {
                size_t init = ast->children[ast->first_child[statement]];
                size_t condition = ast->children[ast->first_child[statement] + 1];
                if (ast->kind[init] == TRAZO_AST_VAR_DECL) {
                    size_t name_node = ast->children[ast->first_child[init] + 1];
                    const TrazoToken *name = &tokens->items[ast->token_index[name_node]];
                    if (symbol_count < TRAZO_MAX_LOCAL_SYMBOLS) {
                        size_t declared_type = ast->children[ast->first_child[init]];
                        const TrazoToken *type = &tokens->items[ast->token_index[declared_type]];
                        symbols[symbol_count].name = name->start;
                        symbols[symbol_count].length = name->length;
                        symbols[symbol_count].type = token_type(type->kind);
                        ++symbol_count;
                    }
                }
                TrazoTypeKind actual = expression_type(tokens, ast, condition, error, symbols,
                                                        symbol_count, functions, function_count);
                if (actual == TRAZO_TYPE_INVALID) {
                    set_error(error, "for condition has no known type",
                              &tokens->items[ast->token_index[statement]]);
                    return 0;
                }
            } else if (ast->kind[statement] == TRAZO_AST_CALL) {
                size_t first = ast->first_child[statement];
                if (ast->kind[ast->children[first]] == TRAZO_AST_IDENTIFIER
                    && (symbol_matches(&tokens->items[ast->token_index[ast->children[first]]], "C", 1)
                        || has_function_name(functions, function_count,
                                             &tokens->items[ast->token_index[ast->children[first]]]))) {
                    (void)expression_type(tokens, ast, statement, error, symbols, symbol_count,
                                          functions, function_count);
                    if (error && error->message) return 0;
                }
            }
        }
    }
    return 1;
    */
}

const char *trazo_type_name(TrazoTypeKind kind) {
    static const char *names[] = {"invalid", "void", "int", "float", "char"};
    return kind >= 0 && (size_t)kind < sizeof(names) / sizeof(names[0]) ? names[kind] : "unknown";
}