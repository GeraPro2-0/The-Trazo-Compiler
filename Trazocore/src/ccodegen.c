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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

typedef struct {
    const TrazoTokenList *tokens;
    const TrazoAst *ast;
    Buffer *buffer;
    TrazoParserError *error;
} Generator;

static const char *c_type(TrazoTokenKind kind);

static size_t find_function_by_name(const TrazoTokenList *tokens, const TrazoAst *ast,
                                    const TrazoToken *name) {
    size_t program = ast->node_count - 1;
    size_t result = SIZE_MAX;
    size_t index;
    if (ast->kind[program] != TRAZO_AST_PROGRAM) return SIZE_MAX;
    for (index = 0; index < ast->child_count[program]; ++index) {
        size_t node = ast->children[ast->first_child[program] + index];
        size_t function_name;
        const TrazoToken *candidate;
        if (ast->kind[node] != TRAZO_AST_FUNCTION) continue;
        function_name = ast->children[ast->first_child[node]];
        candidate = &tokens->items[ast->token_index[function_name]];
        if (candidate->length == name->length
            && memcmp(candidate->start, name->start, name->length) == 0) {
            if (result != SIZE_MAX) return SIZE_MAX;
            result = node;
        }
    }
    return result;
}

static int append_text(Buffer *buffer, const char *text, size_t length) {
    size_t required = buffer->length + length + 1;
    size_t capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    char *data;
    while (capacity < required) capacity *= 2;
    if (capacity != buffer->capacity) {
        data = realloc(buffer->data, capacity);
        if (!data) return 0;
        buffer->data = data;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int append_format(Buffer *buffer, const char *format, ...) {
    va_list arguments;
    va_list copy;
    int length;
    char *text;
    va_start(arguments, format);
    va_copy(copy, arguments);
    length = vsnprintf(NULL, 0, format, arguments);
    va_end(arguments);
    if (length < 0) {
        va_end(copy);
        return 0;
    }
    text = malloc((size_t)length + 1);
    if (!text) {
        va_end(copy);
        return 0;
    }
    vsnprintf(text, (size_t)length + 1, format, copy);
    va_end(copy);
    if (!append_text(buffer, text, (size_t)length)) {
        free(text);
        return 0;
    }
    free(text);
    return 1;
}

static void set_error(Generator *generator, const char *message, size_t node) {
    const TrazoToken *token = &generator->tokens->items[generator->ast->token_index[node]];
    if (generator->error && !generator->error->message) {
        generator->error->message = message;
        generator->error->line = token->line;
        generator->error->column = token->column;
    }
}

static int append_token(Buffer *buffer, const TrazoToken *token) {
    return append_text(buffer, token->start, token->length);
}

static int emit_expression_inner(Generator *generator, size_t node);

static int emit_expression(Generator *generator, size_t node) {
    if (generator->ast->flags[node] & TRAZO_AST_FLAG_GROUPED) {
        if (!append_text(generator->buffer, "(", 1)
            || !emit_expression_inner(generator, node)) return 0;
        return append_text(generator->buffer, ")", 1);
    }
    return emit_expression_inner(generator, node);
}

static int emit_expression_inner(Generator *generator, size_t node) {
    const TrazoAst *ast = generator->ast;
    const TrazoTokenList *tokens = generator->tokens;
    const TrazoToken *token = &tokens->items[ast->token_index[node]];
    size_t first = ast->first_child[node];
    size_t count = ast->child_count[node];
    size_t index;

    if (ast->kind[node] == TRAZO_AST_IDENTIFIER || ast->kind[node] == TRAZO_AST_INTEGER
        || ast->kind[node] == TRAZO_AST_FLOAT || ast->kind[node] == TRAZO_AST_STRING
        || ast->kind[node] == TRAZO_AST_CHARACTER) {
        return append_token(generator->buffer, token);
    }

    if (ast->kind[node] == TRAZO_AST_SIZEOF) {
        size_t operand = ast->children[first];
        if (count != 1) {
            set_error(generator, "invalid sizeof node", node);
            return 0;
        }
        if (!append_text(generator->buffer, "sizeof(", 7)) return 0;
        if (ast->kind[operand] == TRAZO_AST_IDENTIFIER) {
            const char *type = c_type(tokens->items[ast->token_index[operand]].kind);
            if (type && !append_text(generator->buffer, type, strlen(type))) return 0;
            if (!type && !emit_expression(generator, operand)) return 0;
        } else if (!emit_expression(generator, operand)) {
            return 0;
        }
        return append_text(generator->buffer, ")", 1);
    }

    if (ast->kind[node] == TRAZO_AST_CAST) {
        const char *type = c_type(token->kind);
        if (count != 1 || !type) {
            set_error(generator, "invalid cast node", node);
            return 0;
        }
        if (!append_text(generator->buffer, "(", 1)
            || !append_text(generator->buffer, type, strlen(type))
            || ((ast->flags[node] & TRAZO_AST_FLAG_POINTER)
                && !append_text(generator->buffer, "*", 1))
            || !append_text(generator->buffer, ")", 1)
            || !emit_expression(generator, ast->children[first])) return 0;
        return 1;
    }

    if (ast->kind[node] == TRAZO_AST_UNARY) {
        if (count != 1) {
            set_error(generator, "invalid unary node", node);
            return 0;
        }
        if (ast->flags[node] & TRAZO_AST_FLAG_PREFIX) {
            if (!append_token(generator->buffer, token)
                || !emit_expression(generator, ast->children[first])) return 0;
            return 1;
        }
        if (!emit_expression(generator, ast->children[first])
            || !append_token(generator->buffer, token)) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_BINARY) {
        if (count != 2) {
            set_error(generator, "invalid binary node", node);
            return 0;
        }
        if (!emit_expression(generator, ast->children[first])
            || !append_text(generator->buffer, " ", 1)) return 0;
        if (!append_token(generator->buffer, token)) return 0;
        if (!append_text(generator->buffer, " ", 1)) return 0;
        return emit_expression(generator, ast->children[first + 1]);
    }
    if (ast->kind[node] == TRAZO_AST_CALL && count >= 3
        && ast->kind[ast->children[first]] == TRAZO_AST_IDENTIFIER
        && tokens->items[ast->token_index[ast->children[first]]].length == 1
        && tokens->items[ast->token_index[ast->children[first]]].start[0] == 'C'
        && ast->kind[ast->children[first + 1]] == TRAZO_AST_IDENTIFIER) {
        if (!emit_expression(generator, ast->children[first + 1])) return 0;
        if (!append_text(generator->buffer, "(", 1)) return 0;
        for (index = 2; index < count; ++index) {
            if (index > 2 && !append_text(generator->buffer, ", ", 2)) return 0;
            if (!emit_expression(generator, ast->children[first + index])) return 0;
        }
        return append_text(generator->buffer, ")", 1);
    }
    if (ast->kind[node] == TRAZO_AST_CALL && count == 2
        && ast->resolved_function[node] == SIZE_MAX
        && (tokens->items[ast->token_index[node]].kind == TRAZO_TOKEN_RIGHT_BRACKET
            || tokens->items[ast->token_index[node]].kind == TRAZO_TOKEN_DOT
            || tokens->items[ast->token_index[node]].kind == TRAZO_TOKEN_ARROW)) {
        if (tokens->items[ast->token_index[node]].kind == TRAZO_TOKEN_RIGHT_BRACKET) {
            if (!emit_expression(generator, ast->children[first])
                || !append_text(generator->buffer, "[", 1)
                || !emit_expression(generator, ast->children[first + 1])
                || !append_text(generator->buffer, "]", 1)) return 0;
            return 1;
        }
        if (!emit_expression(generator, ast->children[first])
            || !append_text(generator->buffer,
                tokens->items[ast->token_index[node]].kind == TRAZO_TOKEN_ARROW ? "->" : ".",
                tokens->items[ast->token_index[node]].kind == TRAZO_TOKEN_ARROW ? 2u : 1u)
            || !emit_expression(generator, ast->children[first + 1])) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_CALL && count >= 1) {
        size_t callee = ast->children[first];
        if (ast->kind[callee] == TRAZO_AST_IDENTIFIER) {
            size_t function = ast->resolved_function[node];
            if (function != SIZE_MAX) {
                char *mangled_name = trazo_mangle_function(tokens, ast, function);
                if (!mangled_name) return 0;
                if (!append_text(generator->buffer, mangled_name, strlen(mangled_name))) {
                    free(mangled_name);
                    return 0;
                }
                free(mangled_name);
            } else {
                const TrazoToken *callee_token = &tokens->items[ast->token_index[callee]];
                function = find_function_by_name(tokens, ast, callee_token);
                if (function != SIZE_MAX) {
                    char *mangled_name = trazo_mangle_function(tokens, ast, function);
                    if (!mangled_name
                        || !append_text(generator->buffer, mangled_name, strlen(mangled_name))) {
                        free(mangled_name);
                        return 0;
                    }
                    free(mangled_name);
                } else if (!emit_expression(generator, callee)) {
                    return 0;
                }
            }
            if (!append_text(generator->buffer, "(", 1)) return 0;
            for (index = 1; index < count; ++index) {
                if (index > 1 && !append_text(generator->buffer, ", ", 2)) return 0;
                if (!emit_expression(generator, ast->children[first + index])) return 0;
            }
            return append_text(generator->buffer, ")", 1);
        }
    }
    if (ast->kind[node] != TRAZO_AST_CALL) return append_token(generator->buffer, token);
    if (count < 2) {
        set_error(generator, "invalid call node", node);
        return 0;
    }
    if (ast->kind[ast->children[first]] == TRAZO_AST_IDENTIFIER
        && tokens->items[ast->token_index[ast->children[first]]].length == 1
        && tokens->items[ast->token_index[ast->children[first]]].start[0] == 'C') {
        if (!append_token(generator->buffer, &tokens->items[ast->token_index[ast->children[first + 1]]])) return 0;
    } else {
        if (!emit_expression(generator, ast->children[first])) return 0;
        if (!append_text(generator->buffer, ".", 1)
            || !emit_expression(generator, ast->children[first + 1])) return 0;
    }
    if (!append_text(generator->buffer, "(", 1)) return 0;
    for (index = 2; index < count; ++index) {
        if (index > 2 && !append_text(generator->buffer, ", ", 2)) return 0;
        if (!emit_expression(generator, ast->children[first + index])) return 0;
    }
    return append_text(generator->buffer, ")", 1);
}

static int emit_indent(Buffer *buffer, int indent) {
    int index;
    for (index = 0; index < indent; ++index) {
        if (!append_text(buffer, "    ", 4)) return 0;
    }
    return 1;
}

static int emit_statement(Generator *generator, size_t node, int indent) {
    const TrazoAst *ast = generator->ast;
    size_t first = ast->first_child[node];
    size_t count = ast->child_count[node];
    size_t index;

    if (ast->kind[node] == TRAZO_AST_VAR_DECL) {
        size_t type = ast->children[first];
        size_t name = ast->children[first + 1];
        const TrazoTokenList *tokens = generator->tokens;
        const TrazoToken *type_token = &tokens->items[ast->token_index[type]];
        const char *declared_type = c_type(type_token->kind);
        if (!emit_indent(generator->buffer, indent)) return 0;
        if (ast->flags[node] & TRAZO_AST_FLAG_CONST && !append_text(generator->buffer, "const ", strlen("const "))) return 0;
        if (ast->flags[node] & TRAZO_AST_FLAG_STATIC && !append_text(generator->buffer, "static ", strlen("static "))) return 0;
        if (ast->flags[node] & TRAZO_AST_FLAG_VOLATILE && !append_text(generator->buffer, "volatile ", strlen("volatile "))) return 0;
        if (declared_type) {
            if (!append_text(generator->buffer, declared_type, strlen(declared_type))) return 0;
        } else {
            if (!append_text(generator->buffer, type_token->start, type_token->length)) return 0;
        }
        if (!append_text(generator->buffer,
                 ast->flags[node] & TRAZO_AST_FLAG_POINTER ? " *" : " ",
                 ast->flags[node] & TRAZO_AST_FLAG_POINTER ? 2u : 1u)
            || !append_text(generator->buffer, tokens->items[ast->token_index[name]].start,
                            tokens->items[ast->token_index[name]].length)) return 0;
        if (ast->flags[node] & TRAZO_AST_FLAG_ARRAY) {
            if (!append_text(generator->buffer, "[", 1)
                || !emit_expression(generator, ast->children[first + 2])
                || !append_text(generator->buffer, "]", 1)) return 0;
        }
        if (count >= ((ast->flags[node] & TRAZO_AST_FLAG_ARRAY) ? 4u : 3u)) {
            size_t initializer = ast->children[first +
                ((ast->flags[node] & TRAZO_AST_FLAG_ARRAY) ? 3u : 2u)];
            if (!append_text(generator->buffer, " = ", strlen(" = "))
                || !emit_expression(generator, initializer)
                || !append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
            return 1;
        }
        if (!append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_TYPEDEF) {
        const TrazoTokenList *tokens = generator->tokens;
        size_t base_type = ast->children[first];
        size_t alias_name = ast->children[first + 1];
        const TrazoToken *base = &tokens->items[ast->token_index[base_type]];
        const TrazoToken *alias = &tokens->items[ast->token_index[alias_name]];
        const char *base_type_name = c_type(base->kind);
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "typedef ", strlen("typedef "))
            || !(base_type_name
                ? append_text(generator->buffer, base_type_name, strlen(base_type_name))
                : append_text(generator->buffer, base->start, base->length))
            || !append_text(generator->buffer, " ", 1)
            || !append_text(generator->buffer, alias->start, alias->length)
            || !append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_STRUCT_DECL || ast->kind[node] == TRAZO_AST_UNION_DECL) {
        const TrazoTokenList *tokens = generator->tokens;
        const char *keyword = ast->kind[node] == TRAZO_AST_STRUCT_DECL ? "struct" : "union";
        size_t name = ast->children[first];
        if (!emit_indent(generator->buffer, indent)) return 0;
        if (name != SIZE_MAX && tokens->items[ast->token_index[name]].length > 0) {
            if (!append_text(generator->buffer, "typedef ", strlen("typedef "))
                || !append_text(generator->buffer, keyword, strlen(keyword))
                || !append_text(generator->buffer, " ", 1)
                || !append_token(generator->buffer, &tokens->items[ast->token_index[name]])) return 0;
            if (!append_text(generator->buffer, " {\n", strlen(" {\n"))) return 0;
            for (index = 1; index < count; ++index) {
                size_t field = ast->children[first + index];
                if (ast->kind[field] == TRAZO_AST_VAR_DECL) {
                    size_t field_type = ast->children[ast->first_child[field]];
                    size_t field_name = ast->children[ast->first_child[field] + 1];
                    const TrazoToken *ft = &tokens->items[ast->token_index[field_type]];
                    const char *field_decl_type = c_type(ft->kind);
                    if (!emit_indent(generator->buffer, indent + 1)) return 0;
                    if (field_decl_type) {
                        if (!append_text(generator->buffer, field_decl_type, strlen(field_decl_type))) return 0;
                    } else {
                        if (!append_text(generator->buffer, ft->start, ft->length)) return 0;
                    }
                    if (!append_text(generator->buffer,
                                     ast->flags[field] & TRAZO_AST_FLAG_POINTER ? " *" : " ",
                                     ast->flags[field] & TRAZO_AST_FLAG_POINTER ? 2u : 1u)
                        || !append_text(generator->buffer, tokens->items[ast->token_index[field_name]].start,
                                        tokens->items[ast->token_index[field_name]].length)
                        || ((ast->flags[field] & TRAZO_AST_FLAG_ARRAY)
                            && (!append_text(generator->buffer, "[", 1)
                                || !emit_expression(generator, ast->children[ast->first_child[field] + 2])
                                || !append_text(generator->buffer, "]", 1)))
                        || !append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
                }
            }
            if (!emit_indent(generator->buffer, indent)
                || !append_text(generator->buffer, "} ", strlen("} "))
                || !append_token(generator->buffer, &tokens->items[ast->token_index[name]])
                || !append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
            return 1;
        }
        if (!append_text(generator->buffer, keyword, strlen(keyword))
            || !append_text(generator->buffer, " {\n", strlen(" {\n"))) return 0;
        for (index = 1; index < count; ++index) {
            size_t field = ast->children[first + index];
            if (ast->kind[field] == TRAZO_AST_VAR_DECL) {
                size_t field_type = ast->children[ast->first_child[field]];
                size_t field_name = ast->children[ast->first_child[field] + 1];
                const TrazoToken *ft = &tokens->items[ast->token_index[field_type]];
                const char *field_decl_type = c_type(ft->kind);
                if (!emit_indent(generator->buffer, indent + 1)) return 0;
                if (field_decl_type) {
                    if (!append_text(generator->buffer, field_decl_type, strlen(field_decl_type))) return 0;
                } else {
                    if (!append_text(generator->buffer, ft->start, ft->length)) return 0;
                }
                if (!append_text(generator->buffer,
                                 ast->flags[field] & TRAZO_AST_FLAG_POINTER ? " *" : " ",
                                 ast->flags[field] & TRAZO_AST_FLAG_POINTER ? 2u : 1u)
                    || !append_text(generator->buffer, tokens->items[ast->token_index[field_name]].start,
                                    tokens->items[ast->token_index[field_name]].length)
                    || ((ast->flags[field] & TRAZO_AST_FLAG_ARRAY)
                        && (!append_text(generator->buffer, "[", 1)
                            || !emit_expression(generator, ast->children[ast->first_child[field] + 2])
                            || !append_text(generator->buffer, "]", 1)))
                    || !append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
            }
        }
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "};\n", strlen("};\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_ENUM_DECL) {
        size_t name = ast->children[first];
        const TrazoTokenList *tokens = generator->tokens;
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "enum ", strlen("enum "))
            || !append_token(generator->buffer, &tokens->items[ast->token_index[name]])
            || !append_text(generator->buffer, " { ", strlen(" { "))) return 0;
        for (index = 1; index < count; ++index) {
            if (index > 1 && !append_text(generator->buffer, ", ", strlen(", "))) return 0;
            if (!append_token(generator->buffer, &tokens->items[ast->token_index[ast->children[first + index]]])) return 0;
        }
        if (!append_text(generator->buffer, " };\n", strlen(" };\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_RETURN) {
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "return", strlen("return"))) return 0;
        if (count > 0 && (!append_text(generator->buffer, " ", 1)
                || !emit_expression(generator, ast->children[first]))) return 0;
        if (!append_text(generator->buffer, ";\n", strlen(";\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_BREAK) {
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "break;\n", strlen("break;\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_CONTINUE) {
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "continue;\n", strlen("continue;\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_UNSAFE) {
        size_t inner = ast->children[first];
        for (index = 0; index < ast->child_count[inner]; ++index) {
            if (!emit_statement(generator, ast->children[ast->first_child[inner] + index], indent)) return 0;
        }
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_SWITCH) {
        size_t condition = ast->children[first];
        size_t case_count = ast->child_count[node] - 1;
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "switch (", strlen("switch ("))
            || !emit_expression(generator, condition)
            || !append_text(generator->buffer, ") {\n", strlen(") {\n"))) return 0;
        for (index = 0; index < case_count; ++index) {
            size_t case_node = ast->children[first + 1 + index];
            if (ast->kind[case_node] == TRAZO_AST_CASE) {
                if (!emit_indent(generator->buffer, indent + 1)
                    || !append_text(generator->buffer, "case ", strlen("case "))
                    || !emit_expression(generator, ast->children[ast->first_child[case_node]])
                    || !append_text(generator->buffer, ":\n", strlen(":\n"))) return 0;
                {
                    size_t body = ast->children[ast->first_child[case_node] + 1];
                    for (size_t stmt_index = 0; stmt_index < ast->child_count[body]; ++stmt_index) {
                        if (!emit_statement(generator, ast->children[ast->first_child[body] + stmt_index], indent + 2)) return 0;
                    }
                }
            } else if (ast->kind[case_node] == TRAZO_AST_DEFAULT) {
                if (!emit_indent(generator->buffer, indent + 1)
                    || !append_text(generator->buffer, "default:\n", strlen("default:\n"))) return 0;
                {
                    size_t body = ast->children[ast->first_child[case_node]];
                    for (size_t stmt_index = 0; stmt_index < ast->child_count[body]; ++stmt_index) {
                        if (!emit_statement(generator, ast->children[ast->first_child[body] + stmt_index], indent + 2)) return 0;
                    }
                }
            }
        }
        if (!emit_indent(generator->buffer, indent) || !append_text(generator->buffer, "}\n", strlen("}\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_IF) {
        size_t condition = ast->children[first];
        size_t then_block = ast->children[first + 1];
        size_t else_block = count > 2 ? ast->children[first + 2] : SIZE_MAX;
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "if (", strlen("if ("))
            || !emit_expression(generator, condition)
            || !append_text(generator->buffer, ") {\n", strlen(") {\n"))) return 0;
        for (index = 0; index < ast->child_count[then_block]; ++index) {
            if (!emit_statement(generator, ast->children[ast->first_child[then_block] + index], indent + 1)) return 0;
        }
        if (!emit_indent(generator->buffer, indent) || !append_text(generator->buffer, "}\n", strlen("}\n"))) return 0;
        if (else_block != SIZE_MAX) {
            if (!emit_indent(generator->buffer, indent)
                || !append_text(generator->buffer, "else {\n", strlen("else {\n"))) return 0;
            for (index = 0; index < ast->child_count[else_block]; ++index) {
                if (!emit_statement(generator, ast->children[ast->first_child[else_block] + index], indent + 1)) return 0;
            }
            if (!emit_indent(generator->buffer, indent) || !append_text(generator->buffer, "}\n", strlen("}\n"))) return 0;
        }
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_WHILE) {
        size_t condition = ast->children[first];
        size_t loop_block = ast->children[first + 1];
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "while (", strlen("while ("))
            || !emit_expression(generator, condition)
            || !append_text(generator->buffer, ") {\n", strlen(") {\n"))) return 0;
        for (index = 0; index < ast->child_count[loop_block]; ++index) {
            if (!emit_statement(generator, ast->children[ast->first_child[loop_block] + index], indent + 1)) return 0;
        }
        if (!emit_indent(generator->buffer, indent) || !append_text(generator->buffer, "}\n", strlen("}\n"))) return 0;
        return 1;
    }
    if (ast->kind[node] == TRAZO_AST_FOR) {
        size_t init = ast->children[first];
        size_t condition = ast->children[first + 1];
        size_t update = ast->children[first + 2];
        size_t loop_block = ast->children[first + 3];
        if (!emit_indent(generator->buffer, indent)
            || !append_text(generator->buffer, "for (", strlen("for ("))) return 0;
        if (ast->kind[init] == TRAZO_AST_VAR_DECL) {
            size_t type = ast->children[ast->first_child[init]];
            size_t name = ast->children[ast->first_child[init] + 1];
            size_t value = ast->children[ast->first_child[init] + 2];
            const char *declared_type = c_type(generator->tokens->items[ast->token_index[type]].kind);
            if (!declared_type
                || !append_format(generator->buffer, "%s %.*s = ", declared_type,
                                  (int)generator->tokens->items[ast->token_index[name]].length,
                                  generator->tokens->items[ast->token_index[name]].start)
                || !emit_expression(generator, value)) return 0;
        } else if (!emit_expression(generator, init)) {
            return 0;
        }
        if (!append_text(generator->buffer, "; ", strlen("; "))
            || !emit_expression(generator, condition)
            || !append_text(generator->buffer, "; ", strlen("; "))
            || !emit_expression(generator, update)
            || !append_text(generator->buffer, ") {\n", strlen(") {\n"))) return 0;
        for (index = 0; index < ast->child_count[loop_block]; ++index) {
            if (!emit_statement(generator, ast->children[ast->first_child[loop_block] + index], indent + 1)) return 0;
        }
        if (!emit_indent(generator->buffer, indent) || !append_text(generator->buffer, "}\n", strlen("}\n"))) return 0;
        return 1;
    }
    if (!emit_indent(generator->buffer, indent)) return 0;
    return emit_expression(generator, node) && append_text(generator->buffer, ";\n", 2);
}

static const char *c_type(TrazoTokenKind kind) {
    switch (kind) {
        case TRAZO_TOKEN_KW_VOID: return "void";
        case TRAZO_TOKEN_KW_INT: return "int";
        case TRAZO_TOKEN_KW_FLOAT: return "float";
        case TRAZO_TOKEN_KW_CHAR: return "char";
        default: return NULL;
    }
}

int trazo_codegen_c(const TrazoTokenList *tokens, const TrazoAst *ast,
                    const TrazoModuleImportList *modules, const char *header_path,
                    TrazoGeneratedC *output,
                    TrazoParserError *error) {
    Buffer buffer = {0};
    size_t index;
    if (!tokens || !ast || !output || ast->node_count == 0) return 0;
    *output = (TrazoGeneratedC){0};
    if (error) *error = (TrazoParserError){0};
    if (modules) {
        for (index = 0; index < modules->count; ++index) {
            const TrazoModuleImport *module = &modules->items[index];
            if (module->kind == TRAZO_MODULE_C_HEADER) {
                if (!append_format(&buffer, "#include <%.*s>\n", (int)module->module_length, module->module)) goto fail;
            } else if (module->kind == TRAZO_MODULE_C_SOURCE) {
                if (!append_format(&buffer, "#include %.*s\n", (int)module->module_length, module->module)) goto fail;
            } else if (module->kind == TRAZO_MODULE_SOURCE) {
                if (!append_format(&buffer, "#include \"%.*s.h\"\n",
                                   (int)module->module_length, module->module)) goto fail;
            }
        }
    }
    if (buffer.length > 0 && !append_text(&buffer, "\n", 1)) goto fail;
    if (header_path) {
        const char *header_name = strrchr(header_path, '\\');
        const char *slash = strrchr(header_path, '/');
        if (slash && (!header_name || slash > header_name)) header_name = slash;
        header_name = header_name ? header_name + 1 : header_path;
        if (!append_format(&buffer, "#include \"%s\"\n\n", header_name)) goto fail;
    }
    if (ast->kind[ast->node_count - 1] != TRAZO_AST_PROGRAM) goto fail;
    for (index = 0; index < ast->child_count[ast->node_count - 1]; ++index) {
        size_t function = ast->children[ast->first_child[ast->node_count - 1] + index];
        if (ast->kind[function] == TRAZO_AST_VAR_DECL
            || ast->kind[function] == TRAZO_AST_TYPEDEF
            || ast->kind[function] == TRAZO_AST_ENUM_DECL
            || ast->kind[function] == TRAZO_AST_STRUCT_DECL
            || ast->kind[function] == TRAZO_AST_UNION_DECL) {
            if (ast->flags[function] & TRAZO_AST_FLAG_EXPORT) continue;
            if (!emit_statement(&(Generator){tokens, ast, &buffer, error}, function, 0)) goto fail;
            continue;
        }
        size_t type = ast->children[ast->first_child[function] + 1];
        size_t params = ast->children[ast->first_child[function] + 2];
        size_t block = ast->children[ast->first_child[function] + 3];
        const char *return_type = c_type(tokens->items[ast->token_index[type]].kind);
        size_t statement;
        if (ast->kind[function] != TRAZO_AST_FUNCTION || !return_type) goto fail;
        {
            char *mangled_name = trazo_mangle_function(tokens, ast, function);
            if (!mangled_name || ((ast->flags[function] & TRAZO_AST_FLAG_DECLARATION)
                    && !append_text(&buffer, "extern ", 7))
                || !append_format(&buffer, "%s ", return_type)
                || !append_text(&buffer, mangled_name, strlen(mangled_name))
                || !append_text(&buffer, "(", 1)) {
                free(mangled_name);
                goto fail;
            }
            free(mangled_name);
        }
        if (ast->child_count[params] > 0) {
            for (statement = 0; statement < ast->child_count[params]; ++statement) {
                size_t param = ast->children[ast->first_child[params] + statement];
                size_t param_type = ast->children[ast->first_child[param]];
                size_t param_name = ast->children[ast->first_child[param] + 1];
                const char *declared_type = c_type(tokens->items[ast->token_index[param_type]].kind);
                if (!declared_type) goto fail;
                if (statement > 0 && !append_text(&buffer, ", ", 2)) goto fail;
                if (!append_format(&buffer, "%s %s%.*s", declared_type,
                                   ast->flags[param] & TRAZO_AST_FLAG_POINTER ? "*" : "",
                                   (int)tokens->items[ast->token_index[param_name]].length,
                                   tokens->items[ast->token_index[param_name]].start)) goto fail;
            }
        } else if (!append_text(&buffer, "void", 4)) {
            goto fail;
        }
        if ((ast->flags[function] & TRAZO_AST_FLAG_DECLARATION)
            && !append_text(&buffer, ");\n", 3)) goto fail;
        if (ast->flags[function] & TRAZO_AST_FLAG_DECLARATION) continue;
        if (!append_text(&buffer, ") {\n", 4)) goto fail;
        for (statement = 0; statement < ast->child_count[block]; ++statement) {
            size_t node = ast->children[ast->first_child[block] + statement];
            if (!emit_statement(&(Generator){tokens, ast, &buffer, error}, node, 1)) goto fail;
        }
        if (!append_text(&buffer, "}\n", 2)) goto fail;
    }
    output->source = buffer.data;
    output->length = buffer.length;
    return 1;
fail:
    free(buffer.data);
    return 0;
}

int trazo_codegen_h(const TrazoTokenList *tokens, const TrazoAst *ast,
                    const char *header_path, TrazoGeneratedC *output,
                    TrazoParserError *error) {
    Buffer buffer = {0};
    char guard[256] = "TRAZO_";
    size_t guard_length = 6;
    const char *header_name = strrchr(header_path ? header_path : "", '\\');
    const char *slash = strrchr(header_path ? header_path : "", '/');
    size_t index;
    size_t name_index;
    if (slash && (!header_name || slash > header_name)) header_name = slash;
    header_name = header_name ? header_name + 1 : (header_path ? header_path : "module.h");
    for (name_index = 0; header_name[name_index] && header_name[name_index] != '.'
        && guard_length + 2 < sizeof(guard); ++name_index) {
        char character = header_name[name_index];
        if (character >= 'a' && character <= 'z') character = (char)(character - 'a' + 'A');
        if (!((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')))
            character = '_';
        guard[guard_length++] = character;
    }
    guard[guard_length++] = '_';
    guard[guard_length++] = 'H';
    guard[guard_length] = '\0';
    if (!tokens || !ast || !output || !header_path || ast->node_count == 0) return 0;
    *output = (TrazoGeneratedC){0};
    if (error) *error = (TrazoParserError){0};
    if (!append_format(&buffer, "#ifndef %s\n#define %s\n\n", guard, guard)) goto fail;
    if (ast->kind[ast->node_count - 1] != TRAZO_AST_PROGRAM) goto fail;
    for (index = 0; index < ast->child_count[ast->node_count - 1]; ++index) {
        size_t node = ast->children[ast->first_child[ast->node_count - 1] + index];
        Generator generator = {tokens, ast, &buffer, error};
        if (!(ast->flags[node] & TRAZO_AST_FLAG_EXPORT)) continue;
        if (ast->kind[node] == TRAZO_AST_TYPEDEF
            || ast->kind[node] == TRAZO_AST_ENUM_DECL
            || ast->kind[node] == TRAZO_AST_STRUCT_DECL
            || ast->kind[node] == TRAZO_AST_UNION_DECL) {
            if (!emit_statement(&generator, node, 0)) goto fail;
            continue;
        }
        if (ast->kind[node] == TRAZO_AST_FUNCTION) {
            size_t first = ast->first_child[node];
            size_t type = ast->children[first + 1];
            size_t params = ast->children[first + 2];
            const char *return_type = c_type(tokens->items[ast->token_index[type]].kind);
            char *function_name = trazo_mangle_function(tokens, ast, node);
            size_t parameter;
            if (!return_type || !function_name
                || !append_format(&buffer, "%s %s(", return_type, function_name)) {
                free(function_name);
                goto fail;
            }
            free(function_name);
            for (parameter = 0; parameter < ast->child_count[params]; ++parameter) {
                size_t param = ast->children[ast->first_child[params] + parameter];
                size_t param_type = ast->children[ast->first_child[param]];
                size_t param_name = ast->children[ast->first_child[param] + 1];
                const char *param_type_name = c_type(tokens->items[ast->token_index[param_type]].kind);
                if (parameter > 0 && !append_text(&buffer, ", ", 2)) goto fail;
                if (!param_type_name || !append_format(&buffer, "%s %s%.*s", param_type_name,
                    ast->flags[param] & TRAZO_AST_FLAG_POINTER ? "*" : "",
                    (int)tokens->items[ast->token_index[param_name]].length,
                    tokens->items[ast->token_index[param_name]].start)) goto fail;
            }
            if (ast->child_count[params] == 0 && !append_text(&buffer, "void", 4)) goto fail;
            if (!append_text(&buffer, ");\n#define ", 11)
                || !append_token(&buffer, &tokens->items[ast->token_index[
                    ast->children[first]]])
                || !append_text(&buffer, " ", 1)) goto fail;
            {
                char *alias_name = trazo_mangle_function(tokens, ast, node);
                if (!alias_name || !append_text(&buffer, alias_name, strlen(alias_name))
                    || !append_text(&buffer, "\n", 1)) {
                    free(alias_name);
                    goto fail;
                }
                free(alias_name);
            }
        }
    }
    if (!append_text(&buffer, "\n#endif\n", 8)) goto fail;
    output->source = buffer.data;
    output->length = buffer.length;
    return 1;
fail:
    free(buffer.data);
    return 0;
}

void trazo_generated_c_free(TrazoGeneratedC *output) {
    if (output) {
        free(output->source);
        *output = (TrazoGeneratedC){0};
    }
}