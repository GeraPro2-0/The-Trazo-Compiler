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

#ifndef TRAZO_CORE_H
#define TRAZO_CORE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TRAZO_TOKEN_EOF = 0,
    TRAZO_TOKEN_ERROR,

    TRAZO_TOKEN_IDENTIFIER,
    TRAZO_TOKEN_INT_LITERAL,
    TRAZO_TOKEN_FLOAT_LITERAL,
    TRAZO_TOKEN_STRING_LITERAL,
    TRAZO_TOKEN_CHAR_LITERAL,
    TRAZO_TOKEN_TRUE,
    TRAZO_TOKEN_FALSE,

    TRAZO_TOKEN_TYPE_INT,
    TRAZO_TOKEN_TYPE_I8,
    TRAZO_TOKEN_TYPE_I16,
    TRAZO_TOKEN_TYPE_I32,
    TRAZO_TOKEN_TYPE_I64,
    TRAZO_TOKEN_TYPE_U8,
    TRAZO_TOKEN_TYPE_U16,
    TRAZO_TOKEN_TYPE_U32,
    TRAZO_TOKEN_TYPE_U64,
    TRAZO_TOKEN_TYPE_BYTE,
    TRAZO_TOKEN_TYPE_CHAR,
    TRAZO_TOKEN_TYPE_RUNE,
    TRAZO_TOKEN_TYPE_F32,
    TRAZO_TOKEN_TYPE_F64,
    TRAZO_TOKEN_TYPE_DECIMAL,
    TRAZO_TOKEN_TYPE_DEC32,
    TRAZO_TOKEN_TYPE_DEC64,
    TRAZO_TOKEN_TYPE_DEC128,
    TRAZO_TOKEN_TYPE_PTR,
    TRAZO_TOKEN_TYPE_FLOAT,
    TRAZO_TOKEN_TYPE_DOUBLE,
    TRAZO_TOKEN_TYPE_BOOL,
    TRAZO_TOKEN_TYPE_STRING,
    TRAZO_TOKEN_TYPE_VOID,

    TRAZO_TOKEN_IF,
    TRAZO_TOKEN_ELIF,
    TRAZO_TOKEN_ELSE,
    TRAZO_TOKEN_WHILE,
    TRAZO_TOKEN_FOR,
    TRAZO_TOKEN_STRUCT,
    TRAZO_TOKEN_FUNC,
    TRAZO_TOKEN_EXTERN,
    TRAZO_TOKEN_RETURN,
    TRAZO_TOKEN_BREAK,
    TRAZO_TOKEN_CONTINUE,

    TRAZO_TOKEN_UNSAFE_MAX,

    TRAZO_TOKEN_MALLOC,
    TRAZO_TOKEN_ALLOC,
    TRAZO_TOKEN_CALLOC,
    TRAZO_TOKEN_REALLOC,
    TRAZO_TOKEN_FREE,
    TRAZO_TOKEN_DELETE,
    TRAZO_TOKEN_SIZEOF,
    TRAZO_TOKEN_PRINT,
    TRAZO_TOKEN_READ_LINE,
    TRAZO_TOKEN_READ_FILE,
    TRAZO_TOKEN_WRITE_FILE,
    TRAZO_TOKEN_APPEND_FILE,
    TRAZO_TOKEN_FILE_EXISTS,
    TRAZO_TOKEN_LEN,
    TRAZO_TOKEN_MEMCPY,
    TRAZO_TOKEN_MEMSET,
    TRAZO_TOKEN_MEMCMP,
    TRAZO_TOKEN_VOLATILE,
    TRAZO_TOKEN_AS,
    TRAZO_TOKEN_NULL,

    TRAZO_TOKEN_PLUS,
    TRAZO_TOKEN_MINUS,
    TRAZO_TOKEN_STAR,
    TRAZO_TOKEN_SLASH,
    TRAZO_TOKEN_PERCENT,
    TRAZO_TOKEN_EQUAL,
    TRAZO_TOKEN_EQUAL_EQUAL,
    TRAZO_TOKEN_BANG_EQUAL,
    TRAZO_TOKEN_LESS,
    TRAZO_TOKEN_LESS_EQUAL,
    TRAZO_TOKEN_GREATER,
    TRAZO_TOKEN_GREATER_EQUAL,
    TRAZO_TOKEN_AMP_AMP,
    TRAZO_TOKEN_PIPE_PIPE,
    TRAZO_TOKEN_BANG,
    TRAZO_TOKEN_AMPERSAND,
    TRAZO_TOKEN_PIPE,
    TRAZO_TOKEN_CARET,
    TRAZO_TOKEN_TILDE,
    TRAZO_TOKEN_LESS_LESS,
    TRAZO_TOKEN_GREATER_GREATER,
    TRAZO_TOKEN_ARROW,
    TRAZO_TOKEN_AT,

    TRAZO_TOKEN_LEFT_PAREN,
    TRAZO_TOKEN_RIGHT_PAREN,
    TRAZO_TOKEN_LEFT_BRACE,
    TRAZO_TOKEN_RIGHT_BRACE,
    TRAZO_TOKEN_LEFT_BRACKET,
    TRAZO_TOKEN_RIGHT_BRACKET,
    TRAZO_TOKEN_COMMA,
    TRAZO_TOKEN_SEMICOLON,
    TRAZO_TOKEN_COLON,
    TRAZO_TOKEN_DOT
} TrazoTokenType;

typedef struct {
    TrazoTokenType type;
    const char* start;
    size_t length;
    int line;
    int column;
} TrazoToken;

typedef struct {
    TrazoToken* items;
    size_t count;
    size_t capacity;
} TrazoTokenList;

typedef enum {
    TRAZO_TYPE_UNKNOWN = 0,
    TRAZO_TYPE_VOID,
    TRAZO_TYPE_INT,
    TRAZO_TYPE_I8,
    TRAZO_TYPE_I16,
    TRAZO_TYPE_I32,
    TRAZO_TYPE_I64,
    TRAZO_TYPE_U8,
    TRAZO_TYPE_U16,
    TRAZO_TYPE_U32,
    TRAZO_TYPE_U64,
    TRAZO_TYPE_BYTE,
    TRAZO_TYPE_CHAR,
    TRAZO_TYPE_RUNE,
    TRAZO_TYPE_F32,
    TRAZO_TYPE_F64,
    TRAZO_TYPE_DECIMAL,
    TRAZO_TYPE_DEC32,
    TRAZO_TYPE_DEC64,
    TRAZO_TYPE_DEC128,
    TRAZO_TYPE_PTR,
    TRAZO_TYPE_FLOAT,
    TRAZO_TYPE_DOUBLE,
    TRAZO_TYPE_BOOL,
    TRAZO_TYPE_STRING,
    TRAZO_TYPE_STRUCT
} TrazoTypeKind;

typedef struct {
    TrazoTypeKind kind;
    char* name;
    unsigned pointer_depth;
    unsigned is_volatile : 1;
} TrazoType;

typedef enum {
    TRAZO_EXPR_LITERAL = 1,
    TRAZO_EXPR_IDENTIFIER,
    TRAZO_EXPR_TYPE,
    TRAZO_EXPR_BINARY,
    TRAZO_EXPR_UNARY,
    TRAZO_EXPR_ASSIGN,
    TRAZO_EXPR_CALL,
    TRAZO_EXPR_FIELD,
    TRAZO_EXPR_ARRAY,
    TRAZO_EXPR_INDEX,
    TRAZO_EXPR_CAST
} TrazoExprKind;

typedef enum {
    TRAZO_STMT_EXPR = 1,
    TRAZO_STMT_VAR,
    TRAZO_STMT_BLOCK,
    TRAZO_STMT_IF,
    TRAZO_STMT_WHILE,
    TRAZO_STMT_FOR,
    TRAZO_STMT_STRUCT,
    TRAZO_STMT_FUNC,
    TRAZO_STMT_EXTERN,
    TRAZO_STMT_UNSAFE,
    TRAZO_STMT_RETURN,
    TRAZO_STMT_BREAK,
    TRAZO_STMT_CONTINUE
} TrazoStmtKind;

typedef struct TrazoExpr TrazoExpr;
typedef struct TrazoStmt TrazoStmt;

struct TrazoExpr {
    TrazoExprKind kind;
    TrazoToken token;
    TrazoType type;
    TrazoExpr* left;
    TrazoExpr* right;
    TrazoExpr** args;
    size_t arg_count;
};

struct TrazoStmt {
    TrazoStmtKind kind;
    TrazoToken token;
    TrazoType type;
    TrazoExpr* expr;
    TrazoExpr* expr2;
    TrazoStmt* init;
    TrazoStmt* body;
    TrazoStmt* else_branch;
    char** param_names;
    TrazoType* param_types;
    size_t param_count;
    TrazoStmt** children;
    size_t child_count;
    size_t child_capacity;
    int is_unsafe;
};

typedef struct {
    TrazoStmt** statements;
    size_t count;
    size_t capacity;
} TrazoProgram;

typedef struct {
    char message[256];
    int line;
    int column;
} TrazoParseError;

TrazoTokenList trazo_lex_source(const char* source);
void trazo_free_tokens(TrazoTokenList tokens);

TrazoProgram* trazo_parse_tokens(const TrazoToken* tokens, size_t count, TrazoParseError* error);
void trazo_free_program(TrazoProgram* program);

int trazo_execute(TrazoProgram* program);

/* Runtime and memory lifecycle helpers (bootstrap-friendly stubs) */
int trazo_memory_init(void);
void trazo_memory_shutdown(void);

int trazo_runtime_init(void);
void trazo_runtime_destroy(void);

/* Centralized allocation wrappers for future SMM integration */
void* trazo_alloc(size_t size);
void* trazo_realloc(void* ptr, size_t size);
void trazo_free(void* ptr);
void* trazo_calloc(size_t nmemb, size_t size);


#endif
