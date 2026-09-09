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
#include <stdint.h>

typedef struct {
    const TrazoTokenList *tokens;
    TrazoAst *ast;
    size_t current;
    TrazoParserError *error;
} Parser;

static void parser_error(Parser *parser, const char *message) {
    const TrazoToken *token = &parser->tokens->items[parser->current];
    if (parser->error && !parser->error->message) {
        parser->error->message = message;
        parser->error->line = token->line;
        parser->error->column = token->column;
    }
}

static int check(const Parser *parser, TrazoTokenKind kind) {
    return parser->tokens->items[parser->current].kind == kind;
}

static TrazoTokenKind peek_kind(const Parser *parser, size_t offset) {
    size_t index = parser->current + offset;
    return index < parser->tokens->count ? parser->tokens->items[index].kind : TRAZO_TOKEN_EOF;
}

static int match(Parser *parser, TrazoTokenKind kind) {
    if (!check(parser, kind)) return 0;
    ++parser->current;
    return 1;
}

static int expect(Parser *parser, TrazoTokenKind kind, const char *message) {
    if (match(parser, kind)) return 1;
    parser_error(parser, message);
    return 0;
}

static int grow_nodes(TrazoAst *ast) {
    size_t capacity = ast->node_capacity == 0 ? 32 : ast->node_capacity * 2;
    TrazoAstKind *kind = malloc(capacity * sizeof(*kind));
    unsigned int *flags = malloc(capacity * sizeof(*flags));
    size_t *first_child = malloc(capacity * sizeof(*first_child));
    size_t *child_count = malloc(capacity * sizeof(*child_count));
    size_t *token_index = malloc(capacity * sizeof(*token_index));
    size_t *resolved_function = malloc(capacity * sizeof(*resolved_function));
    if (!kind || !flags || !first_child || !child_count || !token_index || !resolved_function) {
        free(kind);
        free(flags);
        free(first_child);
        free(child_count);
        free(token_index);
        free(resolved_function);
        return 0;
    }
    if (ast->node_count > 0) {
        memcpy(kind, ast->kind, ast->node_count * sizeof(*kind));
        memcpy(flags, ast->flags, ast->node_count * sizeof(*flags));
        memcpy(first_child, ast->first_child, ast->node_count * sizeof(*first_child));
        memcpy(child_count, ast->child_count, ast->node_count * sizeof(*child_count));
        memcpy(token_index, ast->token_index, ast->node_count * sizeof(*token_index));
         memcpy(resolved_function, ast->resolved_function,
             ast->node_count * sizeof(*resolved_function));
    }
    free(ast->kind);
    free(ast->flags);
    free(ast->first_child);
    free(ast->child_count);
    free(ast->token_index);
    free(ast->resolved_function);
    ast->kind = kind;
    ast->flags = flags;
    ast->first_child = first_child;
    ast->child_count = child_count;
    ast->token_index = token_index;
    ast->resolved_function = resolved_function;
    ast->node_capacity = capacity;
    return 1;
}

static int grow_children(TrazoAst *ast, size_t additional) {
    size_t required = ast->child_count_total + additional;
    size_t capacity = ast->child_capacity == 0 ? 64 : ast->child_capacity;
    size_t *children;
    while (capacity < required) capacity *= 2;
    if (capacity == ast->child_capacity) return 1;
    children = realloc(ast->children, capacity * sizeof(*children));
    if (!children) return 0;
    ast->children = children;
    ast->child_capacity = capacity;
    return 1;
}

static size_t add_node(Parser *parser, TrazoAstKind kind, size_t token_index,
                       const size_t *children, size_t child_count) {
    TrazoAst *ast = parser->ast;
    size_t node;
    if (ast->node_count == ast->node_capacity && !grow_nodes(ast)) {
        parser_error(parser, "out of memory");
        return SIZE_MAX;
    }
    if (!grow_children(ast, child_count)) {
        parser_error(parser, "out of memory");
        return SIZE_MAX;
    }
    node = ast->node_count++;
    ast->kind[node] = kind;
    ast->flags[node] = 0;
    ast->first_child[node] = ast->child_count_total;
    ast->child_count[node] = child_count;
    ast->token_index[node] = token_index;
    ast->resolved_function[node] = SIZE_MAX;
    for (size_t index = 0; index < child_count; ++index) {
        ast->children[ast->child_count_total++] = children[index];
    }
    return node;
}

static size_t parse_expression(Parser *parser);
static size_t parse_unary(Parser *parser);

static size_t parse_primary(Parser *parser) {
    const TrazoToken *token = &parser->tokens->items[parser->current];
    TrazoAstKind kind;
    size_t node;
    if (token->kind == TRAZO_TOKEN_IDENTIFIER && token->length == 6
        && memcmp(token->start, "sizeof", 6) == 0) {
        size_t sizeof_token = parser->current++;
        size_t operand;
        if (!expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after sizeof")) return SIZE_MAX;
        operand = parse_expression(parser);
        if (operand == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_PAREN,
                                            "expected ')' after sizeof")) return SIZE_MAX;
        return add_node(parser, TRAZO_AST_SIZEOF, sizeof_token, &operand, 1);
    }
    if (match(parser, TRAZO_TOKEN_LEFT_PAREN)) {
        TrazoTokenKind type_kind = parser->tokens->items[parser->current].kind;
        if (type_kind == TRAZO_TOKEN_KW_INT || type_kind == TRAZO_TOKEN_KW_FLOAT
            || type_kind == TRAZO_TOKEN_KW_CHAR || type_kind == TRAZO_TOKEN_KW_VOID) {
            size_t type_token = parser->current++;
            int pointer = match(parser, TRAZO_TOKEN_STAR);
            if (match(parser, TRAZO_TOKEN_RIGHT_PAREN)) {
                size_t operand = parse_unary(parser);
                size_t cast;
                if (operand == SIZE_MAX) return SIZE_MAX;
                cast = add_node(parser, TRAZO_AST_CAST, type_token, &operand, 1);
                if (cast == SIZE_MAX) return SIZE_MAX;
                if (pointer) parser->ast->flags[cast] |= TRAZO_AST_FLAG_POINTER;
                return cast;
            }
            parser->current = type_token;
        }
        size_t expression = parse_expression(parser);
        if (expression == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')'")) return SIZE_MAX;
        parser->ast->flags[expression] |= TRAZO_AST_FLAG_GROUPED;
        return expression;
    }
    if (token->kind == TRAZO_TOKEN_IDENTIFIER || token->kind == TRAZO_TOKEN_KW_INT
        || token->kind == TRAZO_TOKEN_KW_FLOAT || token->kind == TRAZO_TOKEN_KW_CHAR
        || token->kind == TRAZO_TOKEN_KW_VOID) kind = TRAZO_AST_IDENTIFIER;
    else if (token->kind == TRAZO_TOKEN_INTEGER) kind = TRAZO_AST_INTEGER;
    else if (token->kind == TRAZO_TOKEN_FLOAT) kind = TRAZO_AST_FLOAT;
    else if (token->kind == TRAZO_TOKEN_STRING) kind = TRAZO_AST_STRING;
    else if (token->kind == TRAZO_TOKEN_CHARACTER) kind = TRAZO_AST_CHARACTER;
    else {
        parser_error(parser, "expected expression");
        return SIZE_MAX;
    }
    ++parser->current;
    node = add_node(parser, kind, parser->current - 1, NULL, 0);
    if (node == SIZE_MAX) return SIZE_MAX;
    return node;
}

static size_t parse_postfix(Parser *parser) {
    size_t expr = parse_primary(parser);
    size_t call_token;
    if (expr == SIZE_MAX) return SIZE_MAX;
    while (1) {
        if (match(parser, TRAZO_TOKEN_LEFT_PAREN)) {
            size_t arguments[32];
            size_t argument_count = 1;
            size_t call;
            call_token = parser->current - 1;
            arguments[0] = expr;
            if (!check(parser, TRAZO_TOKEN_RIGHT_PAREN)) {
                while (1) {
                    if (argument_count == sizeof(arguments) / sizeof(arguments[0])) {
                        parser_error(parser, "too many call arguments");
                        return SIZE_MAX;
                    }
                    arguments[argument_count++] = parse_expression(parser);
                    if (arguments[argument_count - 1] == SIZE_MAX) return SIZE_MAX;
                    if (!match(parser, TRAZO_TOKEN_COMMA)) break;
                }
            }
            if (!expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')'")) return SIZE_MAX;
            call = add_node(parser, TRAZO_AST_CALL, call_token, arguments, argument_count);
            if (call == SIZE_MAX) return SIZE_MAX;
            expr = call;
            continue;
        }
        if (check(parser, TRAZO_TOKEN_DOT) || check(parser, TRAZO_TOKEN_ARROW)) {
            TrazoTokenKind access_kind = parser->tokens->items[parser->current].kind;
            size_t dot_token = parser->current - 1;
            ++parser->current;
            dot_token = parser->current - 1;
            size_t member = parse_primary(parser);
            size_t arguments[66];
            size_t argument_count = 2;
            size_t call;
            if (member == SIZE_MAX) return SIZE_MAX;
            if (check(parser, TRAZO_TOKEN_LEFT_PAREN)) {
                if (!expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '('")) return SIZE_MAX;
                arguments[0] = expr;
                arguments[1] = member;
                while (!check(parser, TRAZO_TOKEN_RIGHT_PAREN) && !check(parser, TRAZO_TOKEN_EOF)) {
                    if (argument_count == sizeof(arguments) / sizeof(arguments[0])) {
                        parser_error(parser, "too many call arguments");
                        return SIZE_MAX;
                    }
                    arguments[argument_count++] = parse_expression(parser);
                    if (arguments[argument_count - 1] == SIZE_MAX) return SIZE_MAX;
                    if (!match(parser, TRAZO_TOKEN_COMMA)) break;
                }
                if (!expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')'")) return SIZE_MAX;
                call = add_node(parser, TRAZO_AST_CALL, dot_token, arguments, argument_count);
                if (call == SIZE_MAX) return SIZE_MAX;
                expr = call;
                continue;
            }
            arguments[0] = expr;
            arguments[1] = member;
            call = add_node(parser, TRAZO_AST_CALL, dot_token, arguments, argument_count);
            if (call == SIZE_MAX) return SIZE_MAX;
            (void)access_kind;
            expr = call;
            continue;
        }
        if (match(parser, TRAZO_TOKEN_LEFT_BRACKET)) {
            size_t index = parse_expression(parser);
            size_t access;
            if (index == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_BRACKET,
                                             "expected ']'")) return SIZE_MAX;
            access = add_node(parser, TRAZO_AST_CALL, parser->current - 1,
                              (size_t[]){expr, index}, 2);
            if (access == SIZE_MAX) return SIZE_MAX;
            expr = access;
            continue;
        }
        break;
    }
    return expr;
}

static size_t parse_unary(Parser *parser) {
    const TrazoToken *token;
    size_t operand;
    size_t node;
    if (check(parser, TRAZO_TOKEN_PLUS_PLUS)) {
        token = &parser->tokens->items[parser->current];
        ++parser->current;
        operand = parse_unary(parser);
        if (operand == SIZE_MAX) return SIZE_MAX;
        node = add_node(parser, TRAZO_AST_UNARY, (size_t)(token - parser->tokens->items), (size_t[]){operand}, 1);
        if (node == SIZE_MAX) return SIZE_MAX;
        parser->ast->flags[node] |= TRAZO_AST_FLAG_PREFIX;
        return node;
    }
    if (check(parser, TRAZO_TOKEN_MINUS_MINUS)) {
        token = &parser->tokens->items[parser->current];
        ++parser->current;
        operand = parse_unary(parser);
        if (operand == SIZE_MAX) return SIZE_MAX;
        node = add_node(parser, TRAZO_AST_UNARY, (size_t)(token - parser->tokens->items), (size_t[]){operand}, 1);
        if (node == SIZE_MAX) return SIZE_MAX;
        parser->ast->flags[node] |= TRAZO_AST_FLAG_PREFIX;
        return node;
    }
    if (check(parser, TRAZO_TOKEN_BANG)) {
        token = &parser->tokens->items[parser->current];
        ++parser->current;
        operand = parse_unary(parser);
        if (operand == SIZE_MAX) return SIZE_MAX;
        node = add_node(parser, TRAZO_AST_UNARY, (size_t)(token - parser->tokens->items), (size_t[]){operand}, 1);
        if (node == SIZE_MAX) return SIZE_MAX;
        parser->ast->flags[node] |= TRAZO_AST_FLAG_PREFIX;
        return node;
    }
    if (check(parser, TRAZO_TOKEN_STAR) || check(parser, TRAZO_TOKEN_AMPERSAND)
        || check(parser, TRAZO_TOKEN_TILDE) || check(parser, TRAZO_TOKEN_PLUS)
        || check(parser, TRAZO_TOKEN_MINUS)) {
        token = &parser->tokens->items[parser->current];
        ++parser->current;
        operand = parse_unary(parser);
        if (operand == SIZE_MAX) return SIZE_MAX;
        node = add_node(parser, TRAZO_AST_UNARY,
                        (size_t)(token - parser->tokens->items),
                        (size_t[]){operand}, 1);
        if (node == SIZE_MAX) return SIZE_MAX;
        parser->ast->flags[node] |= TRAZO_AST_FLAG_PREFIX;
        return node;
    }
    operand = parse_postfix(parser);
    if (operand == SIZE_MAX) return SIZE_MAX;
    if (check(parser, TRAZO_TOKEN_PLUS_PLUS) || check(parser, TRAZO_TOKEN_MINUS_MINUS)) {
        TrazoTokenKind kind = parser->tokens->items[parser->current].kind;
        size_t op_index = parser->current;
        ++parser->current;
        node = add_node(parser, TRAZO_AST_UNARY, op_index, (size_t[]){operand}, 1);
        if (node == SIZE_MAX) return SIZE_MAX;
        if (kind == TRAZO_TOKEN_PLUS_PLUS) {
            parser->ast->flags[node] &= (unsigned char)~TRAZO_AST_FLAG_PREFIX;
        } else {
            parser->ast->flags[node] &= (unsigned char)~TRAZO_AST_FLAG_PREFIX;
        }
        return node;
    }
    return operand;
}

static int precedence_of(TrazoTokenKind kind) {
    switch (kind) {
        case TRAZO_TOKEN_EQUAL:
            return 1;
        case TRAZO_TOKEN_PLUS_PLUS:
        case TRAZO_TOKEN_MINUS_MINUS:
            return 30;
        case TRAZO_TOKEN_STAR:
        case TRAZO_TOKEN_SLASH:
        case TRAZO_TOKEN_PERCENT:
            return 20;
        case TRAZO_TOKEN_PLUS:
        case TRAZO_TOKEN_MINUS:
            return 10;
        case TRAZO_TOKEN_EQUAL_EQUAL:
        case TRAZO_TOKEN_BANG_EQUAL:
        case TRAZO_TOKEN_LESS:
        case TRAZO_TOKEN_LESS_EQUAL:
        case TRAZO_TOKEN_GREATER:
        case TRAZO_TOKEN_GREATER_EQUAL:
            return 5;
        case TRAZO_TOKEN_AND_AND:
            return 4;
        case TRAZO_TOKEN_OR_OR:
            return 3;
        case TRAZO_TOKEN_BANG:
            return 25;
        default:
            return -1;
    }
}

static size_t parse_binary(Parser *parser, int min_precedence) {
    size_t left = parse_unary(parser);
    size_t operator_token;
    int precedence;
    if (left == SIZE_MAX) return SIZE_MAX;
    while (!check(parser, TRAZO_TOKEN_EOF)
        && !check(parser, TRAZO_TOKEN_SEMICOLON)
        && !check(parser, TRAZO_TOKEN_RIGHT_PAREN)
        && !check(parser, TRAZO_TOKEN_RIGHT_BRACE)
        && !check(parser, TRAZO_TOKEN_COMMA)) {
        precedence = precedence_of(parser->tokens->items[parser->current].kind);
        if (precedence < min_precedence) break;
        operator_token = parser->current;
        ++parser->current;
        {
            size_t right = parse_binary(parser, precedence + 1);
            size_t children[2];
            size_t node;
            if (right == SIZE_MAX) return SIZE_MAX;
            children[0] = left;
            children[1] = right;
            node = add_node(parser, TRAZO_AST_BINARY, operator_token, children, 2);
            if (node == SIZE_MAX) return SIZE_MAX;
            left = node;
        }
    }
    return left;
}

static size_t parse_expression(Parser *parser);
static size_t parse_statement(Parser *parser);

static int is_type_start(const Parser *parser) {
    return check(parser, TRAZO_TOKEN_KW_INT)
        || check(parser, TRAZO_TOKEN_KW_FLOAT)
        || check(parser, TRAZO_TOKEN_KW_CHAR)
        || check(parser, TRAZO_TOKEN_KW_VOID)
        || check(parser, TRAZO_TOKEN_IDENTIFIER);
}

static unsigned char parse_type_modifiers(Parser *parser) {
    unsigned char flags = 0;
    while (1) {
        if (match(parser, TRAZO_TOKEN_KW_CONST)) {
            flags |= TRAZO_AST_FLAG_CONST;
            continue;
        }
        if (match(parser, TRAZO_TOKEN_KW_STATIC)) {
            flags |= TRAZO_AST_FLAG_STATIC;
            continue;
        }
        if (match(parser, TRAZO_TOKEN_KW_VOLATILE)) {
            flags |= TRAZO_AST_FLAG_VOLATILE;
            continue;
        }
        return flags;
    }
}

static size_t parse_expression(Parser *parser) {
    return parse_binary(parser, 0);
}

/*
 * Assignment is parsed as a regular binary operator with the lowest precedence.
 * This keeps the grammar C-like without introducing a separate AST node.
 */

static size_t parse_block(Parser *parser) {
    size_t statements[64];
    size_t count = 0;
    size_t block;
    if (!expect(parser, TRAZO_TOKEN_LEFT_BRACE, "expected '{'")) return SIZE_MAX;
    while (!check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
        if (count == sizeof(statements) / sizeof(statements[0])) {
            parser_error(parser, "too many statements in block");
            return SIZE_MAX;
        }
        statements[count++] = parse_statement(parser);
        if (statements[count - 1] == SIZE_MAX) return SIZE_MAX;
    }
    if (!expect(parser, TRAZO_TOKEN_RIGHT_BRACE, "expected '}'")) return SIZE_MAX;
    block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, statements, count);
    if (block == SIZE_MAX) return SIZE_MAX;
    return block;
}

static size_t parse_switch_cases(Parser *parser, size_t *case_count, size_t case_nodes[32], size_t case_values[32], size_t *default_block) {
    size_t value_count = 0;
    size_t default_index = SIZE_MAX;
    if (!expect(parser, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after 'switch'")) return SIZE_MAX;
    while (!check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
        size_t case_value;
        size_t case_block;
        size_t statements[32];
        size_t statement_count = 0;
        if (match(parser, TRAZO_TOKEN_KW_CASE)) {
            case_value = parse_expression(parser);
            if (case_value == SIZE_MAX || !expect(parser, TRAZO_TOKEN_COLON, "expected ':' after case label")) return SIZE_MAX;
            while (!check(parser, TRAZO_TOKEN_KW_CASE) && !check(parser, TRAZO_TOKEN_KW_DEFAULT)
                && !check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
                if (statement_count == sizeof(statements) / sizeof(statements[0])) {
                    parser_error(parser, "too many statements in case block");
                    return SIZE_MAX;
                }
                statements[statement_count++] = parse_statement(parser);
                if (statements[statement_count - 1] == SIZE_MAX) return SIZE_MAX;
            }
            case_block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, statements, statement_count);
            if (case_block == SIZE_MAX) return SIZE_MAX;
            case_nodes[value_count] = add_node(parser, TRAZO_AST_CASE, parser->current - 1, (size_t[]){case_value, case_block}, 2);
            if (case_nodes[value_count] == SIZE_MAX) return SIZE_MAX;
            case_values[value_count] = case_value;
            value_count++;
            continue;
        }
        if (match(parser, TRAZO_TOKEN_KW_DEFAULT)) {
            if (!expect(parser, TRAZO_TOKEN_COLON, "expected ':' after 'default'")) return SIZE_MAX;
            while (!check(parser, TRAZO_TOKEN_KW_CASE) && !check(parser, TRAZO_TOKEN_KW_DEFAULT)
                && !check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
                if (statement_count == sizeof(statements) / sizeof(statements[0])) {
                    parser_error(parser, "too many statements in default block");
                    return SIZE_MAX;
                }
                statements[statement_count++] = parse_statement(parser);
                if (statements[statement_count - 1] == SIZE_MAX) return SIZE_MAX;
            }
            case_block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, statements, statement_count);
            if (case_block == SIZE_MAX) return SIZE_MAX;
            default_index = value_count;
            case_nodes[value_count] = add_node(parser, TRAZO_AST_DEFAULT, parser->current - 1, (size_t[]){case_block}, 1);
            if (case_nodes[value_count] == SIZE_MAX) return SIZE_MAX;
            value_count++;
            continue;
        }
        parser_error(parser, "expected 'case' or 'default' in switch");
        return SIZE_MAX;
    }
    if (!expect(parser, TRAZO_TOKEN_RIGHT_BRACE, "expected '}' after switch body")) return SIZE_MAX;
    *case_count = value_count;
    *default_block = default_index;
    return 0;
}

static size_t parse_statement(Parser *parser) {
    size_t expression;
    size_t children[8];
    size_t return_token;
    size_t type;
    size_t name;
    size_t declaration_children[4];
    size_t condition;
    size_t then_block;
    size_t else_block = SIZE_MAX;
    size_t loop_block;
    if (match(parser, TRAZO_TOKEN_KW_EXPORT)) {
        size_t declaration = parse_statement(parser);
        if (declaration == SIZE_MAX) return SIZE_MAX;
        parser->ast->flags[declaration] |= TRAZO_AST_FLAG_EXPORT;
        return declaration;
    }
    if (match(parser, TRAZO_TOKEN_KW_UNSAFE)) {
        size_t unsafe_block;
        size_t unsafe_children[1];
        size_t unsafe_node;
        unsafe_block = parse_block(parser);
        if (unsafe_block == SIZE_MAX) return SIZE_MAX;
        unsafe_children[0] = unsafe_block;
        unsafe_node = add_node(parser, TRAZO_AST_UNSAFE, parser->current - 1, unsafe_children, 1);
        if (unsafe_node == SIZE_MAX) return SIZE_MAX;
        return unsafe_node;
    }
    if (match(parser, TRAZO_TOKEN_KW_TYPEDEF)) {
        size_t base_type;
        size_t alias_name;
        size_t alias_children[2];
        size_t typedef_node;
        if (!is_type_start(parser)) {
            parser_error(parser, "expected type name in typedef");
            return SIZE_MAX;
        }
        base_type = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
        ++parser->current;
        if (base_type == SIZE_MAX || !check(parser, TRAZO_TOKEN_IDENTIFIER)) {
            parser_error(parser, "expected alias name in typedef");
            return SIZE_MAX;
        }
        alias_name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
        ++parser->current;
        if (alias_name == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after typedef")) return SIZE_MAX;
        alias_children[0] = base_type;
        alias_children[1] = alias_name;
        typedef_node = add_node(parser, TRAZO_AST_TYPEDEF, parser->current - 1, alias_children, 2);
        if (typedef_node == SIZE_MAX) return SIZE_MAX;
        return typedef_node;
    }
    if (check(parser, TRAZO_TOKEN_KW_STRUCT) || check(parser, TRAZO_TOKEN_KW_UNION)) {
        size_t is_union = 0;
        size_t struct_name = SIZE_MAX;
        size_t field_count = 0;
        size_t fields[32];
        size_t decl_children[33];
        size_t decl_node;
        size_t struct_token;
        if (check(parser, TRAZO_TOKEN_KW_UNION)) {
            is_union = 1;
        }
        struct_token = parser->current;
        ++parser->current;
        if (check(parser, TRAZO_TOKEN_IDENTIFIER)) {
            struct_name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
            ++parser->current;
            if (struct_name == SIZE_MAX) return SIZE_MAX;
        }
        if (!expect(parser, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after struct/union name")) return SIZE_MAX;
        while (!check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
            size_t field_type;
            size_t field_name;
            size_t field_decl[3];
            if (!is_type_start(parser)) {
                parser_error(parser, "expected field type in struct/union");
                return SIZE_MAX;
            }
            field_type = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
            ++parser->current;
            int field_pointer = match(parser, TRAZO_TOKEN_STAR);
            if (field_type == SIZE_MAX || !check(parser, TRAZO_TOKEN_IDENTIFIER)) {
                parser_error(parser, "expected field name in struct/union");
                return SIZE_MAX;
            }
            field_name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
            ++parser->current;
            int field_array = 0;
            if (match(parser, TRAZO_TOKEN_LEFT_BRACKET)) {
                field_decl[2] = parse_expression(parser);
                if (field_decl[2] == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_BRACKET,
                                                         "expected ']' after field array size")) return SIZE_MAX;
                field_array = 1;
            }
            if (field_name == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after field declaration")) return SIZE_MAX;
            field_decl[0] = field_type;
            field_decl[1] = field_name;
            fields[field_count++] = add_node(parser, TRAZO_AST_VAR_DECL, parser->current - 1,
                                             field_decl, field_array ? 3u : 2u);
            if (fields[field_count - 1] == SIZE_MAX) return SIZE_MAX;
            if (field_pointer) parser->ast->flags[fields[field_count - 1]] |= TRAZO_AST_FLAG_POINTER;
            if (field_array) parser->ast->flags[fields[field_count - 1]] |= TRAZO_AST_FLAG_ARRAY;
        }
        if (!expect(parser, TRAZO_TOKEN_RIGHT_BRACE, "expected '}' after struct/union definition")) return SIZE_MAX;
        if (!expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after struct/union definition")) return SIZE_MAX;
        decl_children[0] = struct_name != SIZE_MAX ? struct_name : add_node(parser, TRAZO_AST_IDENTIFIER, struct_token, NULL, 0);
        for (size_t index = 0; index < field_count; ++index) {
            decl_children[index + 1] = fields[index];
        }
        decl_node = add_node(parser, is_union ? TRAZO_AST_UNION_DECL : TRAZO_AST_STRUCT_DECL, struct_token, decl_children, field_count + 1);
        if (decl_node == SIZE_MAX) return SIZE_MAX;
        return decl_node;
    }
    if (match(parser, TRAZO_TOKEN_KW_ENUM)) {
        size_t enum_name;
        size_t enum_values[32];
        size_t value_count = 0;
        size_t enum_children[33];
        size_t enum_decl;
        if (!check(parser, TRAZO_TOKEN_IDENTIFIER)) {
            parser_error(parser, "expected enum name");
            return SIZE_MAX;
        }
        enum_name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
        ++parser->current;
        if (enum_name == SIZE_MAX || !expect(parser, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after enum name")) return SIZE_MAX;
        while (!check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
            if (!check(parser, TRAZO_TOKEN_IDENTIFIER)) {
                parser_error(parser, "expected enum member name");
                return SIZE_MAX;
            }
            if (value_count == sizeof(enum_values) / sizeof(enum_values[0])) {
                parser_error(parser, "too many enum members");
                return SIZE_MAX;
            }
            enum_values[value_count++] = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current, NULL, 0);
            ++parser->current;
            if (enum_values[value_count - 1] == SIZE_MAX) return SIZE_MAX;
            if (!match(parser, TRAZO_TOKEN_COMMA)) break;
        }
        if (!expect(parser, TRAZO_TOKEN_RIGHT_BRACE, "expected '}' after enum members")) return SIZE_MAX;
        if (!expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after enum declaration")) return SIZE_MAX;
        enum_children[0] = enum_name;
        for (size_t index = 0; index < value_count; ++index) {
            enum_children[index + 1] = enum_values[index];
        }
        enum_decl = add_node(parser, TRAZO_AST_ENUM_DECL, parser->current - 1, enum_children, value_count + 1);
        if (enum_decl == SIZE_MAX) return SIZE_MAX;
        return enum_decl;
    }
    if (match(parser, TRAZO_TOKEN_KW_BREAK)) {
        size_t break_token = parser->current - 1;
        if (!expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after 'break'")) return SIZE_MAX;
        return add_node(parser, TRAZO_AST_BREAK, break_token, NULL, 0);
    }
    if (match(parser, TRAZO_TOKEN_KW_CONTINUE)) {
        size_t continue_token = parser->current - 1;
        if (!expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after 'continue'")) return SIZE_MAX;
        return add_node(parser, TRAZO_AST_CONTINUE, continue_token, NULL, 0);
    }
    if (match(parser, TRAZO_TOKEN_KW_IF)) {
        if (!expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after 'if'")) return SIZE_MAX;
        condition = parse_expression(parser);
        if (condition == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')'")) return SIZE_MAX;
        then_block = parse_block(parser);
        if (then_block == SIZE_MAX) return SIZE_MAX;
        if (match(parser, TRAZO_TOKEN_KW_ELSE)) {
            if (check(parser, TRAZO_TOKEN_KW_IF)) {
                size_t nested_if = parse_statement(parser);
                if (nested_if == SIZE_MAX) return SIZE_MAX;
                else_block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1,
                                      &nested_if, 1);
            } else {
                else_block = parse_block(parser);
            }
            if (else_block == SIZE_MAX) return SIZE_MAX;
        }
        children[0] = condition;
        children[1] = then_block;
        if (else_block != SIZE_MAX) {
            children[2] = else_block;
            return add_node(parser, TRAZO_AST_IF, parser->current - 1, children, 3);
        }
        return add_node(parser, TRAZO_AST_IF, parser->current - 1, children, 2);
    }
    if (match(parser, TRAZO_TOKEN_KW_WHILE)) {
        if (!expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after 'while'")) return SIZE_MAX;
        condition = parse_expression(parser);
        if (condition == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')'")) return SIZE_MAX;
        loop_block = parse_block(parser);
        if (loop_block == SIZE_MAX) return SIZE_MAX;
        children[0] = condition;
        children[1] = loop_block;
        return add_node(parser, TRAZO_AST_WHILE, parser->current - 1, children, 2);
    }
    if (match(parser, TRAZO_TOKEN_KW_SWITCH)) {
        size_t switch_value;
        size_t case_nodes[32];
        size_t case_count = 0;
        size_t default_block = SIZE_MAX;
        size_t switch_node;
        if (!expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after 'switch'")) return SIZE_MAX;
        switch_value = parse_expression(parser);
        if (switch_value == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after switch value")) return SIZE_MAX;
        if (parse_switch_cases(parser, &case_count, case_nodes, (size_t[32]){0}, &default_block) == SIZE_MAX) return SIZE_MAX;
        children[0] = switch_value;
        for (size_t index = 0; index < case_count; ++index) {
            children[index + 1] = case_nodes[index];
        }
        switch_node = add_node(parser, TRAZO_AST_SWITCH, parser->current - 1, children, case_count + 1);
        if (switch_node == SIZE_MAX) return SIZE_MAX;
        return switch_node;
    }
    if (match(parser, TRAZO_TOKEN_KW_FOR)) {
        size_t init = SIZE_MAX;
        size_t update = SIZE_MAX;
        if (!expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after 'for'")) return SIZE_MAX;
        if (check(parser, TRAZO_TOKEN_KW_INT) || check(parser, TRAZO_TOKEN_KW_FLOAT)
            || check(parser, TRAZO_TOKEN_KW_CHAR)) {
            size_t type_token = parser->current++;
            size_t name_token;
            size_t decl_type = add_node(parser, TRAZO_AST_IDENTIFIER, type_token, NULL, 0);
            size_t decl_name;
            size_t decl_value;
            if (decl_type == SIZE_MAX || !check(parser, TRAZO_TOKEN_IDENTIFIER)) {
                parser_error(parser, "expected variable name in for-init");
                return SIZE_MAX;
            }
            name_token = parser->current++;
            decl_name = add_node(parser, TRAZO_AST_IDENTIFIER, name_token, NULL, 0);
            if (decl_name == SIZE_MAX || !expect(parser, TRAZO_TOKEN_EQUAL, "expected '=' in for-init")) return SIZE_MAX;
            decl_value = parse_expression(parser);
            if (decl_value == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after for-init")) return SIZE_MAX;
            init = add_node(parser, TRAZO_AST_VAR_DECL, type_token, (size_t[]){decl_type, decl_name, decl_value}, 3);
            if (init == SIZE_MAX) return SIZE_MAX;
        } else {
            init = parse_expression(parser);
            if (init == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after for-init")) return SIZE_MAX;
        }
        condition = parse_expression(parser);
        if (condition == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';' after for-condition")) return SIZE_MAX;
        update = parse_expression(parser);
        if (update == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after for-update")) return SIZE_MAX;
        loop_block = parse_block(parser);
        if (loop_block == SIZE_MAX) return SIZE_MAX;
        children[0] = init;
        children[1] = condition;
        children[2] = update;
        children[3] = loop_block;
        return add_node(parser, TRAZO_AST_FOR, parser->current - 1, children, 4);
    }
    if ((check(parser, TRAZO_TOKEN_KW_CONST) || check(parser, TRAZO_TOKEN_KW_STATIC)
        || check(parser, TRAZO_TOKEN_KW_VOLATILE) || check(parser, TRAZO_TOKEN_KW_INT)
        || check(parser, TRAZO_TOKEN_KW_FLOAT) || check(parser, TRAZO_TOKEN_KW_CHAR)
        || check(parser, TRAZO_TOKEN_IDENTIFIER))
        && (check(parser, TRAZO_TOKEN_KW_CONST) || check(parser, TRAZO_TOKEN_KW_STATIC)
        || check(parser, TRAZO_TOKEN_KW_VOLATILE) || check(parser, TRAZO_TOKEN_KW_INT)
        || check(parser, TRAZO_TOKEN_KW_FLOAT) || check(parser, TRAZO_TOKEN_KW_CHAR)
        || (check(parser, TRAZO_TOKEN_IDENTIFIER)
            && (peek_kind(parser, 1) == TRAZO_TOKEN_IDENTIFIER
                || (peek_kind(parser, 1) == TRAZO_TOKEN_STAR
                    && peek_kind(parser, 2) == TRAZO_TOKEN_IDENTIFIER))))) {
        unsigned char modifier_flags = parse_type_modifiers(parser);
        if (!check(parser, TRAZO_TOKEN_KW_INT) && !check(parser, TRAZO_TOKEN_KW_FLOAT)
            && !check(parser, TRAZO_TOKEN_KW_CHAR) && !check(parser, TRAZO_TOKEN_IDENTIFIER)) {
            parser_error(parser, "expected variable type");
            return SIZE_MAX;
        }
        type = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current++, NULL, 0);
        int pointer = match(parser, TRAZO_TOKEN_STAR);
        if (type == SIZE_MAX || !check(parser, TRAZO_TOKEN_IDENTIFIER)) {
            parser_error(parser, "expected variable name");
            return SIZE_MAX;
        }
        name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current++, NULL, 0);
        if (name == SIZE_MAX) return SIZE_MAX;
        declaration_children[0] = type;
        declaration_children[1] = name;
        int array = 0;
        if (match(parser, TRAZO_TOKEN_LEFT_BRACKET)) {
            declaration_children[2] = parse_expression(parser);
            if (declaration_children[2] == SIZE_MAX || !expect(parser, TRAZO_TOKEN_RIGHT_BRACKET,
                                                               "expected ']' after array size")) return SIZE_MAX;
            array = 1;
        }
        if (match(parser, TRAZO_TOKEN_EQUAL)) {
            expression = parse_expression(parser);
            if (expression == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';'")) return SIZE_MAX;
            declaration_children[array ? 3 : 2] = expression;
            {
                size_t var_decl = add_node(parser, TRAZO_AST_VAR_DECL, parser->current - 1,
                                           declaration_children, array ? 4u : 3u);
                if (var_decl == SIZE_MAX) return SIZE_MAX;
                parser->ast->flags[var_decl] |= modifier_flags;
                if (pointer) parser->ast->flags[var_decl] |= TRAZO_AST_FLAG_POINTER;
                if (array) parser->ast->flags[var_decl] |= TRAZO_AST_FLAG_ARRAY;
                return var_decl;
            }
        }
        if (!expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';'")) return SIZE_MAX;
        {
            size_t var_decl = add_node(parser, TRAZO_AST_VAR_DECL, parser->current - 1,
                                       declaration_children, array ? 3u : 2u);
            if (var_decl == SIZE_MAX) return SIZE_MAX;
            parser->ast->flags[var_decl] |= modifier_flags;
            if (pointer) parser->ast->flags[var_decl] |= TRAZO_AST_FLAG_POINTER;
            if (array) parser->ast->flags[var_decl] |= TRAZO_AST_FLAG_ARRAY;
            return var_decl;
        }
    }
    if (match(parser, TRAZO_TOKEN_KW_RETURN)) {
        return_token = parser->current - 1;
        if (match(parser, TRAZO_TOKEN_SEMICOLON)) {
            return add_node(parser, TRAZO_AST_RETURN, return_token, NULL, 0);
        }
        expression = parse_expression(parser);
        if (expression == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';'")) return SIZE_MAX;
        children[0] = expression;
        return add_node(parser, TRAZO_AST_RETURN, return_token, children, 1);
    }
    expression = parse_expression(parser);
    if (expression == SIZE_MAX || !expect(parser, TRAZO_TOKEN_SEMICOLON, "expected ';'")) return SIZE_MAX;
    return expression;
}

static size_t parse_parameter_list(Parser *parser, size_t *param_count, size_t params[32]) {
    size_t count = 0;
    if (match(parser, TRAZO_TOKEN_RIGHT_PAREN)) return 0;
    while (1) {
        size_t type;
        size_t name;
        size_t declaration[3];
        if (!check(parser, TRAZO_TOKEN_KW_INT) && !check(parser, TRAZO_TOKEN_KW_VOID)
            && !check(parser, TRAZO_TOKEN_KW_FLOAT) && !check(parser, TRAZO_TOKEN_KW_CHAR)) {
            parser_error(parser, "expected parameter type");
            return SIZE_MAX;
        }
        type = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current++, NULL, 0);
        int pointer = match(parser, TRAZO_TOKEN_STAR);
        if (type == SIZE_MAX || !check(parser, TRAZO_TOKEN_IDENTIFIER)) {
            parser_error(parser, "expected parameter name");
            return SIZE_MAX;
        }
        name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current++, NULL, 0);
        declaration[0] = type;
        declaration[1] = name;
        params[count++] = add_node(parser, TRAZO_AST_VAR_DECL, parser->current - 1, declaration, 2);
        if (pointer) parser->ast->flags[params[count - 1]] |= TRAZO_AST_FLAG_POINTER;
        if (count == 32) {
            parser_error(parser, "too many parameters");
            return SIZE_MAX;
        }
        if (!match(parser, TRAZO_TOKEN_COMMA)) break;
    }
    if (!expect(parser, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after function arguments")) return SIZE_MAX;
    *param_count = count;
    return 0;
}

static size_t parse_function(Parser *parser) {
    size_t name;
    size_t return_type;
    size_t statements[64];
    size_t count = 0;
    size_t children[8];
    size_t param_nodes[32];
    size_t param_count = 0;
    size_t params_block;
    size_t block;
    int extern_c = 0;
    int exported = 0;
    size_t function_token;
    function_token = parser->current;
    if (match(parser, TRAZO_TOKEN_KW_EXPORT)) exported = 1;
    if (match(parser, TRAZO_TOKEN_KW_EXTERN)) {
        const TrazoToken *linkage = &parser->tokens->items[parser->current];
        if (!match(parser, TRAZO_TOKEN_STRING)
            || linkage->length != 3 || linkage->start[1] != 'C') {
            parser_error(parser, "expected extern \"C\"");
            return SIZE_MAX;
        }
        extern_c = 1;
    }
    if (!expect(parser, TRAZO_TOKEN_KW_FUNC, "expected 'func'")) return SIZE_MAX;
    if (!check(parser, TRAZO_TOKEN_IDENTIFIER)) {
        parser_error(parser, "expected function name");
        return SIZE_MAX;
    }
    name = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current++, NULL, 0);
    if (name == SIZE_MAX || !expect(parser, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after function name")) return SIZE_MAX;
    if (parse_parameter_list(parser, &param_count, param_nodes) == SIZE_MAX) return SIZE_MAX;
    if (!expect(parser, TRAZO_TOKEN_ARROW, "expected '->'")) return SIZE_MAX;
    if (!check(parser, TRAZO_TOKEN_KW_INT) && !check(parser, TRAZO_TOKEN_KW_VOID)
        && !check(parser, TRAZO_TOKEN_KW_FLOAT) && !check(parser, TRAZO_TOKEN_KW_CHAR)) {
        parser_error(parser, "expected return type");
        return SIZE_MAX;
    }
    return_type = add_node(parser, TRAZO_AST_IDENTIFIER, parser->current++, NULL, 0);
    if (return_type == SIZE_MAX) return SIZE_MAX;
    if (match(parser, TRAZO_TOKEN_SEMICOLON)) {
        params_block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, param_nodes, param_count);
        block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, NULL, 0);
        children[0] = name;
        children[1] = return_type;
        children[2] = params_block;
        children[3] = block;
        {
            size_t function = add_node(parser, TRAZO_AST_FUNCTION, function_token, children, 4);
            if (function != SIZE_MAX) {
                parser->ast->flags[function] |= TRAZO_AST_FLAG_DECLARATION;
                if (extern_c) parser->ast->flags[function] |= TRAZO_AST_FLAG_EXTERN_C;
                if (exported) parser->ast->flags[function] |= TRAZO_AST_FLAG_EXPORT;
            }
            return function;
        }
    }
    if (!expect(parser, TRAZO_TOKEN_LEFT_BRACE, "expected '{'")) return SIZE_MAX;
    while (!check(parser, TRAZO_TOKEN_RIGHT_BRACE) && !check(parser, TRAZO_TOKEN_EOF)) {
        if (count == sizeof(statements) / sizeof(statements[0])) {
            parser_error(parser, "too many statements");
            return SIZE_MAX;
        }
        statements[count++] = parse_statement(parser);
        if (statements[count - 1] == SIZE_MAX) return SIZE_MAX;
    }
    if (!expect(parser, TRAZO_TOKEN_RIGHT_BRACE, "expected '}'")) return SIZE_MAX;
    params_block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, param_nodes, param_count);
    block = add_node(parser, TRAZO_AST_BLOCK, parser->current - 1, statements, count);
    children[0] = name;
    children[1] = return_type;
    children[2] = params_block;
    children[3] = block;
    {
        size_t function = add_node(parser, TRAZO_AST_FUNCTION, function_token, children, 4);
        if (function != SIZE_MAX && extern_c) parser->ast->flags[function] |= TRAZO_AST_FLAG_EXTERN_C;
        if (function != SIZE_MAX && exported) parser->ast->flags[function] |= TRAZO_AST_FLAG_EXPORT;
        return function;
    }
}

int trazo_parse(const TrazoTokenList *tokens, TrazoAst *ast, TrazoParserError *error) {
    Parser parser = {tokens, ast, 0, error};
    size_t roots[64];
    size_t count = 0;
    if (!tokens || !ast || tokens->count == 0) return 0;
    *ast = (TrazoAst){0};
    if (error) *error = (TrazoParserError){0};
    while (!check(&parser, TRAZO_TOKEN_EOF)) {
        if (check(&parser, TRAZO_TOKEN_KW_FUNC) || check(&parser, TRAZO_TOKEN_KW_EXTERN)
            || (check(&parser, TRAZO_TOKEN_KW_EXPORT)
                && (peek_kind(&parser, 1) == TRAZO_TOKEN_KW_FUNC
                    || peek_kind(&parser, 1) == TRAZO_TOKEN_KW_EXTERN))) {
            roots[count++] = parse_function(&parser);
        } else {
            roots[count++] = parse_statement(&parser);
            if (roots[count - 1] != SIZE_MAX
                && ast->kind[roots[count - 1]] != TRAZO_AST_VAR_DECL
                && ast->kind[roots[count - 1]] != TRAZO_AST_TYPEDEF
                && ast->kind[roots[count - 1]] != TRAZO_AST_ENUM_DECL
                && ast->kind[roots[count - 1]] != TRAZO_AST_STRUCT_DECL
                && ast->kind[roots[count - 1]] != TRAZO_AST_UNION_DECL) {
                parser_error(&parser, "expected top-level function or variable declaration");
                goto fail;
            }
        }
        if (roots[count - 1] == SIZE_MAX) goto fail;
        if (count == sizeof(roots) / sizeof(roots[0])) {
            parser_error(&parser, "too many top-level declarations");
            goto fail;
        }
    }
    if (add_node(&parser, TRAZO_AST_PROGRAM, 0, roots, count) == SIZE_MAX) goto fail;
    return 1;
fail:
    trazo_ast_free(ast);
    return 0;
}

void trazo_ast_free(TrazoAst *ast) {
    if (ast) {
        free(ast->kind);
        free(ast->flags);
        free(ast->first_child);
        free(ast->child_count);
        free(ast->token_index);
        free(ast->resolved_function);
        free(ast->children);
        *ast = (TrazoAst){0};
    }
}

const char *trazo_ast_name(TrazoAstKind kind) {
    static const char *names[] = {"program", "import_c", "import_module", "from_import",
        "function", "block", "if", "while", "for", "switch", "case", "default",
        "break", "continue", "unsafe", "unary", "call", "binary", "return",
        "var_decl", "identifier", "integer", "float", "string", "character"};
    return kind >= 0 && (size_t)kind < sizeof(names) / sizeof(names[0]) ? names[kind] : "unknown";
}