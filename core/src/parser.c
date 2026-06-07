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
*/

#include "trazo_core.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const TrazoToken* tokens;
    size_t count;
    size_t pos;
    TrazoParseError* error;
    int panic;
    int unsafe_depth;
} Parser;

static int is_at_end(Parser* p) {
    return p->pos >= p->count || p->tokens[p->pos].type == TRAZO_TOKEN_EOF;
}

static const TrazoToken* peek(Parser* p) {
    if (p->pos < p->count) return &p->tokens[p->pos];
    return &p->tokens[p->count - 1];
}

static const TrazoToken* previous(Parser* p) {
    return p->pos > 0 ? &p->tokens[p->pos - 1] : &p->tokens[0];
}

static int check(Parser* p, TrazoTokenType type) {
    return !is_at_end(p) && peek(p)->type == type;
}

static const TrazoToken* advance_token(Parser* p) {
    if (!is_at_end(p)) p->pos++;
    return previous(p);
}

static int match(Parser* p, TrazoTokenType type) {
    if (!check(p, type)) return 0;
    advance_token(p);
    return 1;
}

static void set_error(Parser* p, const TrazoToken* token, const char* message) {
    if (p->panic) return;
    p->panic = 1;
    if (!p->error) return;

    p->error->line = token ? token->line : 0;
    p->error->column = token ? token->column : 0;
    snprintf(p->error->message, sizeof(p->error->message),
             "line %d, column %d: %s", p->error->line, p->error->column, message);
}

static int consume(Parser* p, TrazoTokenType type, const char* message) {
    if (check(p, type)) {
        advance_token(p);
        return 1;
    }
    set_error(p, peek(p), message);
    return 0;
}

static int token_equals(const TrazoToken* token, const char* text) {
    size_t len = strlen(text);
    return token->length == len && memcmp(token->start, text, len) == 0;
}

static char* token_text(const TrazoToken* token) {
    char* text = (char*)trazo_alloc(token->length + 1);
    if (!text) return NULL;
    memcpy(text, token->start, token->length);
    text[token->length] = '\0';
    return text;
}

static int is_unsafe_builtin(const TrazoToken* token) {
    return token_equals(token, "malloc") || token_equals(token, "reservar") ||
           token_equals(token, "alloc") || token_equals(token, "asignar_mem") ||
           token_equals(token, "calloc") || token_equals(token, "asignar_cero") ||
           token_equals(token, "realloc") || token_equals(token, "reasignar") ||
           token_equals(token, "free") || token_equals(token, "liberar") ||
           token_equals(token, "delete") || token_equals(token, "eliminar") ||
           token_equals(token, "memcpy") || token_equals(token, "copiar_mem") ||
           token_equals(token, "memset") || token_equals(token, "llenar_mem") ||
           token_equals(token, "memcmp") || token_equals(token, "comparar_mem");
}

static TrazoExpr* new_expr(TrazoExprKind kind, const TrazoToken* token) {
    TrazoExpr* expr = (TrazoExpr*)trazo_calloc(1, sizeof(TrazoExpr));
    if (!expr) return NULL;
    expr->kind = kind;
    if (token) expr->token = *token;
    return expr;
}

static TrazoStmt* new_stmt(TrazoStmtKind kind, const TrazoToken* token) {
    TrazoStmt* stmt = (TrazoStmt*)trazo_calloc(1, sizeof(TrazoStmt));
    if (!stmt) return NULL;
    stmt->kind = kind;
    if (token) stmt->token = *token;
    return stmt;
}

static int push_stmt(TrazoStmt*** items, size_t* count, size_t* capacity, TrazoStmt* stmt) {
    if (*count == *capacity) {
        size_t next_capacity = *capacity < 8 ? 8 : *capacity * 2;
        TrazoStmt** next_items = (TrazoStmt**)trazo_realloc(*items, next_capacity * sizeof(TrazoStmt*));
        if (!next_items) return 0;
        *items = next_items;
        *capacity = next_capacity;
    }
    (*items)[(*count)++] = stmt;
    return 1;
}

static int push_expr(TrazoExpr*** items, size_t* count, TrazoExpr* expr) {
    size_t next_count = *count + 1;
    TrazoExpr** next_items = (TrazoExpr**)trazo_realloc(*items, next_count * sizeof(TrazoExpr*));
    if (!next_items) return 0;
    *items = next_items;
    (*items)[*count] = expr;
    *count = next_count;
    return 1;
}

static int push_param(TrazoStmt* stmt, TrazoType type, const TrazoToken* name) {
    size_t next_count = stmt->param_count + 1;
    char** next_names = (char**)trazo_realloc(stmt->param_names, next_count * sizeof(char*));
    TrazoType* next_types;
    if (!next_names) return 0;
    stmt->param_names = next_names;

    next_types = (TrazoType*)trazo_realloc(stmt->param_types, next_count * sizeof(TrazoType));
    if (!next_types) return 0;
    stmt->param_types = next_types;

    stmt->param_names[stmt->param_count] = token_text(name);
    stmt->param_types[stmt->param_count] = type;
    stmt->param_count = next_count;
    return 1;
}

static TrazoTypeKind token_to_type(TrazoTokenType type) {
    switch (type) {
        case TRAZO_TOKEN_TYPE_VOID: return TRAZO_TYPE_VOID;
        case TRAZO_TOKEN_TYPE_INT: return TRAZO_TYPE_INT;
        case TRAZO_TOKEN_TYPE_I8: return TRAZO_TYPE_I8;
        case TRAZO_TOKEN_TYPE_I16: return TRAZO_TYPE_I16;
        case TRAZO_TOKEN_TYPE_I32: return TRAZO_TYPE_I32;
        case TRAZO_TOKEN_TYPE_I64: return TRAZO_TYPE_I64;
        case TRAZO_TOKEN_TYPE_U8: return TRAZO_TYPE_U8;
        case TRAZO_TOKEN_TYPE_U16: return TRAZO_TYPE_U16;
        case TRAZO_TOKEN_TYPE_U32: return TRAZO_TYPE_U32;
        case TRAZO_TOKEN_TYPE_U64: return TRAZO_TYPE_U64;
        case TRAZO_TOKEN_TYPE_BYTE: return TRAZO_TYPE_BYTE;
        case TRAZO_TOKEN_TYPE_CHAR: return TRAZO_TYPE_CHAR;
        case TRAZO_TOKEN_TYPE_RUNE: return TRAZO_TYPE_RUNE;
        case TRAZO_TOKEN_TYPE_F32: return TRAZO_TYPE_F32;
        case TRAZO_TOKEN_TYPE_F64: return TRAZO_TYPE_F64;
        case TRAZO_TOKEN_TYPE_DECIMAL: return TRAZO_TYPE_DECIMAL;
        case TRAZO_TOKEN_TYPE_DEC32: return TRAZO_TYPE_DEC32;
        case TRAZO_TOKEN_TYPE_DEC64: return TRAZO_TYPE_DEC64;
        case TRAZO_TOKEN_TYPE_DEC128: return TRAZO_TYPE_DEC128;
        case TRAZO_TOKEN_TYPE_PTR: return TRAZO_TYPE_PTR;
        case TRAZO_TOKEN_TYPE_FLOAT: return TRAZO_TYPE_FLOAT;
        case TRAZO_TOKEN_TYPE_DOUBLE: return TRAZO_TYPE_DOUBLE;
        case TRAZO_TOKEN_TYPE_BOOL: return TRAZO_TYPE_BOOL;
        case TRAZO_TOKEN_TYPE_STRING: return TRAZO_TYPE_STRING;
        default: return TRAZO_TYPE_UNKNOWN;
    }
}

static int is_type_token(TrazoTokenType type) {
    return token_to_type(type) != TRAZO_TYPE_UNKNOWN;
}

static TrazoType parse_type(Parser* p) {
    TrazoType type;
    memset(&type, 0, sizeof(type));
    type.kind = TRAZO_TYPE_UNKNOWN;

    while (match(p, TRAZO_TOKEN_VOLATILE)) {
        type.is_volatile = 1;
    }

    if (is_type_token(peek(p)->type)) {
        type.kind = token_to_type(advance_token(p)->type);
    } else if (check(p, TRAZO_TOKEN_IDENTIFIER)) {
        type.kind = TRAZO_TYPE_STRUCT;
        type.name = token_text(advance_token(p));
    } else {
        set_error(p, peek(p), "expected type");
        return type;
    }

    while (match(p, TRAZO_TOKEN_STAR)) {
        type.pointer_depth++;
    }

    return type;
}

typedef enum {
    PREC_NONE,
    PREC_ASSIGN,
    PREC_OR,
    PREC_AND,
    PREC_BIT_OR,
    PREC_BIT_XOR,
    PREC_BIT_AND,
    PREC_EQUALITY,
    PREC_COMPARE,
    PREC_SHIFT,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL
} Precedence;

static Precedence precedence_of(TrazoTokenType type) {
    switch (type) {
        case TRAZO_TOKEN_EQUAL: return PREC_ASSIGN;
        case TRAZO_TOKEN_PIPE_PIPE: return PREC_OR;
        case TRAZO_TOKEN_AMP_AMP: return PREC_AND;
        case TRAZO_TOKEN_PIPE: return PREC_BIT_OR;
        case TRAZO_TOKEN_CARET: return PREC_BIT_XOR;
        case TRAZO_TOKEN_AMPERSAND: return PREC_BIT_AND;
        case TRAZO_TOKEN_EQUAL_EQUAL:
        case TRAZO_TOKEN_BANG_EQUAL: return PREC_EQUALITY;
        case TRAZO_TOKEN_LESS:
        case TRAZO_TOKEN_LESS_EQUAL:
        case TRAZO_TOKEN_GREATER:
        case TRAZO_TOKEN_GREATER_EQUAL: return PREC_COMPARE;
        case TRAZO_TOKEN_LESS_LESS:
        case TRAZO_TOKEN_GREATER_GREATER: return PREC_SHIFT;
        case TRAZO_TOKEN_PLUS:
        case TRAZO_TOKEN_MINUS: return PREC_TERM;
        case TRAZO_TOKEN_STAR:
        case TRAZO_TOKEN_SLASH:
        case TRAZO_TOKEN_PERCENT: return PREC_FACTOR;
        case TRAZO_TOKEN_LEFT_PAREN:
        case TRAZO_TOKEN_LEFT_BRACKET:
        case TRAZO_TOKEN_DOT:
        case TRAZO_TOKEN_ARROW: return PREC_CALL;
        case TRAZO_TOKEN_AS: return PREC_COMPARE;
        default: return PREC_NONE;
    }
}

static TrazoExpr* parse_expression(Parser* p);
static TrazoExpr* parse_precedence(Parser* p, Precedence min_prec);

static TrazoExpr* parse_primary(Parser* p) {
    if (match(p, TRAZO_TOKEN_INT_LITERAL) ||
        match(p, TRAZO_TOKEN_FLOAT_LITERAL) ||
        match(p, TRAZO_TOKEN_STRING_LITERAL) ||
        match(p, TRAZO_TOKEN_CHAR_LITERAL) ||
        match(p, TRAZO_TOKEN_TRUE) ||
        match(p, TRAZO_TOKEN_FALSE) ||
        match(p, TRAZO_TOKEN_NULL)) {
        return new_expr(TRAZO_EXPR_LITERAL, previous(p));
    }

    if (is_type_token(peek(p)->type)) {
        TrazoExpr* expr = new_expr(TRAZO_EXPR_TYPE, peek(p));
        expr->type = parse_type(p);
        return expr;
    }

    if (match(p, TRAZO_TOKEN_IDENTIFIER) ||
        match(p, TRAZO_TOKEN_MALLOC) ||
        match(p, TRAZO_TOKEN_ALLOC) ||
        match(p, TRAZO_TOKEN_CALLOC) ||
        match(p, TRAZO_TOKEN_REALLOC) ||
        match(p, TRAZO_TOKEN_FREE) ||
        match(p, TRAZO_TOKEN_DELETE) ||
        match(p, TRAZO_TOKEN_SIZEOF) ||
        match(p, TRAZO_TOKEN_PRINT) ||
        match(p, TRAZO_TOKEN_READ_LINE) ||
        match(p, TRAZO_TOKEN_READ_FILE) ||
        match(p, TRAZO_TOKEN_WRITE_FILE) ||
        match(p, TRAZO_TOKEN_APPEND_FILE) ||
        match(p, TRAZO_TOKEN_FILE_EXISTS) ||
        match(p, TRAZO_TOKEN_LEN) ||
        match(p, TRAZO_TOKEN_MEMCPY) ||
        match(p, TRAZO_TOKEN_MEMSET) ||
        match(p, TRAZO_TOKEN_MEMCMP)) {
        return new_expr(TRAZO_EXPR_IDENTIFIER, previous(p));
    }

    if (match(p, TRAZO_TOKEN_LEFT_BRACKET)) {
        TrazoExpr* array = new_expr(TRAZO_EXPR_ARRAY, previous(p));
        if (!check(p, TRAZO_TOKEN_RIGHT_BRACKET)) {
            do {
                if (!push_expr(&array->args, &array->arg_count, parse_expression(p))) {
                    set_error(p, previous(p), "out of memory while parsing array");
                    return array;
                }
            } while (match(p, TRAZO_TOKEN_COMMA));
        }
        consume(p, TRAZO_TOKEN_RIGHT_BRACKET, "expected ']' after array literal");
        return array;
    }

    if (match(p, TRAZO_TOKEN_LEFT_PAREN)) {
        TrazoExpr* expr = parse_expression(p);
        consume(p, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after expression");
        return expr;
    }

    set_error(p, peek(p), "expected expression");
    return NULL;
}

static TrazoExpr* parse_unary(Parser* p) {
    if (match(p, TRAZO_TOKEN_BANG) ||
        match(p, TRAZO_TOKEN_MINUS) ||
        match(p, TRAZO_TOKEN_TILDE) ||
        match(p, TRAZO_TOKEN_STAR) ||
        match(p, TRAZO_TOKEN_AMPERSAND)) {
        const TrazoToken* op = previous(p);
        if ((op->type == TRAZO_TOKEN_STAR || op->type == TRAZO_TOKEN_AMPERSAND) && p->unsafe_depth == 0) {
            set_error(p, op, "pointer operations require unsafe.MAX");
            return NULL;
        }
        TrazoExpr* expr = new_expr(TRAZO_EXPR_UNARY, op);
        expr->right = parse_precedence(p, PREC_UNARY);
        return expr;
    }

    if (match(p, TRAZO_TOKEN_AT)) {
        const TrazoToken* op = previous(p);
        TrazoExpr* expr = new_expr(TRAZO_EXPR_CAST, op);
        expr->type = parse_type(p);
        expr->right = parse_unary(p);
        return expr;
    }

    return parse_primary(p);
}

static TrazoExpr* parse_precedence(Parser* p, Precedence min_prec) {
    TrazoExpr* left = parse_unary(p);

    while (!p->panic) {
        TrazoTokenType op_type = peek(p)->type;
        Precedence prec = precedence_of(op_type);
        if (prec < min_prec || prec == PREC_NONE) break;

        const TrazoToken* op = advance_token(p);

        if (op_type == TRAZO_TOKEN_LEFT_PAREN) {
            TrazoExpr* call = new_expr(TRAZO_EXPR_CALL, op);
            call->left = left;
            if (!check(p, TRAZO_TOKEN_RIGHT_PAREN)) {
                do {
                    if (!push_expr(&call->args, &call->arg_count, parse_expression(p))) {
                        set_error(p, op, "out of memory while parsing call");
                        return call;
                    }
                } while (match(p, TRAZO_TOKEN_COMMA));
            }
            consume(p, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after arguments");
            if (call->left && call->left->kind == TRAZO_EXPR_IDENTIFIER && is_unsafe_builtin(&call->left->token) && p->unsafe_depth == 0) {
                set_error(p, &call->left->token, "unsafe operation requires unsafe.MAX");
                return call;
            }
            left = call;
            continue;
        }

        if (op_type == TRAZO_TOKEN_DOT || op_type == TRAZO_TOKEN_ARROW) {
            consume(p, TRAZO_TOKEN_IDENTIFIER, op_type == TRAZO_TOKEN_DOT ? "expected field name after '.'" : "expected field name after '->'");
            TrazoExpr* field = new_expr(TRAZO_EXPR_FIELD, previous(p));
            field->left = left;
            left = field;
            continue;
        }

        if (op_type == TRAZO_TOKEN_LEFT_BRACKET) {
            TrazoExpr* index = new_expr(TRAZO_EXPR_INDEX, op);
            index->left = left;
            index->right = parse_expression(p);
            consume(p, TRAZO_TOKEN_RIGHT_BRACKET, "expected ']' after index");
            left = index;
            continue;
        }

        if (op_type == TRAZO_TOKEN_AS) {
            TrazoExpr* expr = new_expr(TRAZO_EXPR_CAST, op);
            expr->right = left;
            expr->type = parse_type(p);
            left = expr;
            continue;
        }

        TrazoExpr* expr = new_expr(op_type == TRAZO_TOKEN_EQUAL ? TRAZO_EXPR_ASSIGN : TRAZO_EXPR_BINARY, op);
        expr->left = left;
        expr->right = parse_precedence(p, op_type == TRAZO_TOKEN_EQUAL ? PREC_ASSIGN : (Precedence)(prec + 1));
        left = expr;
    }

    return left;
}

static TrazoExpr* parse_expression(Parser* p) {
    return parse_precedence(p, PREC_ASSIGN);
}

static TrazoStmt* parse_statement(Parser* p);

static TrazoStmt* parse_block(Parser* p, const TrazoToken* token) {
    TrazoStmt* block = new_stmt(TRAZO_STMT_BLOCK, token);
    while (!check(p, TRAZO_TOKEN_RIGHT_BRACE) && !is_at_end(p) && !p->panic) {
        TrazoStmt* child = parse_statement(p);
        if (child && !push_stmt(&block->children, &block->child_count, &block->child_capacity, child)) {
            set_error(p, token, "out of memory while parsing block");
            break;
        }
    }
    consume(p, TRAZO_TOKEN_RIGHT_BRACE, "expected '}' after block");
    return block;
}

static TrazoStmt* parse_var_after_type(Parser* p, TrazoType type, const TrazoToken* token, int need_semicolon) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_VAR, token);
    stmt->type = type;
    consume(p, TRAZO_TOKEN_IDENTIFIER, "expected variable name");
    stmt->token = *previous(p);

    if (match(p, TRAZO_TOKEN_EQUAL)) {
        stmt->expr = parse_expression(p);
    }

    if (need_semicolon) {
        consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after variable declaration");
    }

    return stmt;
}

static TrazoStmt* parse_if(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_IF, token);
    consume(p, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after if");
    stmt->expr = parse_expression(p);
    consume(p, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after if condition");
    consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after if condition");
    stmt->body = parse_block(p, previous(p));

    if (match(p, TRAZO_TOKEN_ELIF)) {
        stmt->else_branch = parse_if(p, previous(p));
    } else if (match(p, TRAZO_TOKEN_ELSE)) {
        consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after else");
        stmt->else_branch = parse_block(p, previous(p));
    }

    return stmt;
}

static TrazoStmt* parse_while(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_WHILE, token);
    consume(p, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after while");
    stmt->expr = parse_expression(p);
    consume(p, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after while condition");
    consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after while condition");
    stmt->body = parse_block(p, previous(p));
    return stmt;
}

static TrazoStmt* parse_for(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_FOR, token);
    consume(p, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after for");

    if (!check(p, TRAZO_TOKEN_SEMICOLON)) {
        if (check(p, TRAZO_TOKEN_VOLATILE) || is_type_token(peek(p)->type) ||
            (check(p, TRAZO_TOKEN_IDENTIFIER) &&
             p->pos + 1 < p->count &&
             p->tokens[p->pos + 1].type == TRAZO_TOKEN_IDENTIFIER)) {
            TrazoType type = parse_type(p);
            stmt->init = parse_var_after_type(p, type, token, 0);
        } else {
            stmt->init = new_stmt(TRAZO_STMT_EXPR, peek(p));
            stmt->init->expr = parse_expression(p);
        }
    }
    consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after for initializer");

    if (!check(p, TRAZO_TOKEN_SEMICOLON)) stmt->expr = parse_expression(p);
    consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after for condition");

    if (!check(p, TRAZO_TOKEN_RIGHT_PAREN)) stmt->expr2 = parse_expression(p);
    consume(p, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after for clauses");

    consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after for clauses");
    stmt->body = parse_block(p, previous(p));
    return stmt;
}

static TrazoStmt* parse_struct(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_STRUCT, token);
    consume(p, TRAZO_TOKEN_IDENTIFIER, "expected struct name");
    stmt->token = *previous(p);
    consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after struct name");

    while (!check(p, TRAZO_TOKEN_RIGHT_BRACE) && !is_at_end(p) && !p->panic) {
        TrazoType field_type = parse_type(p);
        TrazoStmt* field = parse_var_after_type(p, field_type, peek(p), 1);
        if (field && !push_stmt(&stmt->children, &stmt->child_count, &stmt->child_capacity, field)) {
            set_error(p, token, "out of memory while parsing struct");
            break;
        }
    }

    consume(p, TRAZO_TOKEN_RIGHT_BRACE, "expected '}' after struct");
    return stmt;
}

static void parse_params(Parser* p, TrazoStmt* stmt) {
    consume(p, TRAZO_TOKEN_LEFT_PAREN, "expected '(' after function name");
    if (!check(p, TRAZO_TOKEN_RIGHT_PAREN)) {
        do {
            TrazoType type = parse_type(p);
            consume(p, TRAZO_TOKEN_IDENTIFIER, "expected parameter name");
            if (!push_param(stmt, type, previous(p))) {
                set_error(p, previous(p), "out of memory while parsing parameters");
                return;
            }
        } while (match(p, TRAZO_TOKEN_COMMA));
    }
    consume(p, TRAZO_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
}

static void parse_optional_params(Parser* p, TrazoStmt* stmt) {
    if (check(p, TRAZO_TOKEN_LEFT_PAREN)) {
        parse_params(p, stmt);
    }
}

static TrazoStmt* parse_func(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_FUNC, token);
    stmt->type.kind = TRAZO_TYPE_VOID;
    consume(p, TRAZO_TOKEN_IDENTIFIER, "expected function name");
    stmt->token = *previous(p);
    parse_optional_params(p, stmt);
    if (match(p, TRAZO_TOKEN_ARROW)) {
        stmt->type = parse_type(p);
    }
    consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after function signature");
    stmt->body = parse_block(p, previous(p));
    return stmt;
}

static TrazoStmt* parse_extern(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_EXTERN, token);
    if (check(p, TRAZO_TOKEN_STRING_LITERAL)) advance_token(p); /* ABI, e.g. "C". */
    stmt->type = parse_type(p);
    consume(p, TRAZO_TOKEN_IDENTIFIER, "expected extern symbol name");
    stmt->token = *previous(p);
    parse_params(p, stmt);
    consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after extern declaration");
    return stmt;
}

static TrazoStmt* parse_unsafe(Parser* p, const TrazoToken* token) {
    TrazoStmt* stmt = new_stmt(TRAZO_STMT_UNSAFE, token);
    if (match(p, TRAZO_TOKEN_FUNC)) {
        p->unsafe_depth++;
        TrazoStmt* func = parse_func(p, previous(p));
        p->unsafe_depth--;
        if (func) func->is_unsafe = 1;
        trazo_free(stmt);
        return func;
    }
    consume(p, TRAZO_TOKEN_LEFT_BRACE, "expected '{' after unsafe.MAX");
    p->unsafe_depth++;
    stmt->body = parse_block(p, previous(p));
    p->unsafe_depth--;
    return stmt;
}

static TrazoStmt* parse_statement(Parser* p) {
    if (match(p, TRAZO_TOKEN_IF)) return parse_if(p, previous(p));
    if (match(p, TRAZO_TOKEN_WHILE)) return parse_while(p, previous(p));
    if (match(p, TRAZO_TOKEN_FOR)) return parse_for(p, previous(p));
    if (match(p, TRAZO_TOKEN_STRUCT)) return parse_struct(p, previous(p));
    if (match(p, TRAZO_TOKEN_FUNC)) return parse_func(p, previous(p));
    if (match(p, TRAZO_TOKEN_EXTERN)) return parse_extern(p, previous(p));
    if (match(p, TRAZO_TOKEN_UNSAFE_MAX)) return parse_unsafe(p, previous(p));

    if (match(p, TRAZO_TOKEN_LEFT_BRACE)) return parse_block(p, previous(p));

    if (match(p, TRAZO_TOKEN_RETURN)) {
        TrazoStmt* stmt = new_stmt(TRAZO_STMT_RETURN, previous(p));
        if (!check(p, TRAZO_TOKEN_SEMICOLON)) stmt->expr = parse_expression(p);
        consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after return");
        return stmt;
    }

    if (match(p, TRAZO_TOKEN_BREAK)) {
        TrazoStmt* stmt = new_stmt(TRAZO_STMT_BREAK, previous(p));
        consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after break");
        return stmt;
    }

    if (match(p, TRAZO_TOKEN_CONTINUE)) {
        TrazoStmt* stmt = new_stmt(TRAZO_STMT_CONTINUE, previous(p));
        consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after continue");
        return stmt;
    }

    if (check(p, TRAZO_TOKEN_VOLATILE) || is_type_token(peek(p)->type) ||
        (check(p, TRAZO_TOKEN_IDENTIFIER) &&
         p->pos + 1 < p->count &&
         p->tokens[p->pos + 1].type == TRAZO_TOKEN_IDENTIFIER)) {
        const TrazoToken* token = peek(p);
        TrazoType type = parse_type(p);
        return parse_var_after_type(p, type, token, 1);
    }

    TrazoStmt* stmt = new_stmt(TRAZO_STMT_EXPR, peek(p));
    stmt->expr = parse_expression(p);
    consume(p, TRAZO_TOKEN_SEMICOLON, "expected ';' after expression");
    return stmt;
}

TrazoProgram* trazo_parse_tokens(const TrazoToken* tokens, size_t count, TrazoParseError* error) {
    if (error) memset(error, 0, sizeof(*error));
    if (!tokens || count == 0) return NULL;

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.tokens = tokens;
    parser.count = count;
    parser.error = error;

    TrazoProgram* program = (TrazoProgram*)trazo_calloc(1, sizeof(TrazoProgram));
    if (!program) return NULL;

    while (!is_at_end(&parser) && !parser.panic) {
        TrazoStmt* stmt = parse_statement(&parser);
        if (stmt && !push_stmt(&program->statements, &program->count, &program->capacity, stmt)) {
            set_error(&parser, peek(&parser), "out of memory while parsing program");
            break;
        }
    }

    if (parser.panic) {
        trazo_free_program(program);
        return NULL;
    }

    return program;
}

static void free_type(TrazoType* type) {
    if (type && type->name) trazo_free(type->name);
}

static void free_expr(TrazoExpr* expr) {
    if (!expr) return;
    free_expr(expr->left);
    free_expr(expr->right);
    free_type(&expr->type);
    for (size_t i = 0; i < expr->arg_count; i++) free_expr(expr->args[i]);
    trazo_free(expr->args);
    trazo_free(expr);
}

static void free_stmt(TrazoStmt* stmt) {
    if (!stmt) return;
    free_type(&stmt->type);
    free_expr(stmt->expr);
    free_expr(stmt->expr2);
    free_stmt(stmt->init);
    free_stmt(stmt->body);
    free_stmt(stmt->else_branch);
    for (size_t i = 0; i < stmt->param_count; i++) {
        trazo_free(stmt->param_names[i]);
        free_type(&stmt->param_types[i]);
    }
    trazo_free(stmt->param_names);
    trazo_free(stmt->param_types);
    for (size_t i = 0; i < stmt->child_count; i++) free_stmt(stmt->children[i]);
    trazo_free(stmt->children);
    trazo_free(stmt);
}

void trazo_free_program(TrazoProgram* program) {
    if (!program) return;
    for (size_t i = 0; i < program->count; i++) free_stmt(program->statements[i]);
    trazo_free(program->statements);
    trazo_free(program);
}
