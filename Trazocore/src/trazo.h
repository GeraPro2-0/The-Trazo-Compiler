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

#ifndef TRAZO_H
#define TRAZO_H

#include <stddef.h>

typedef enum {
    TRAZO_TOKEN_EOF,
    TRAZO_TOKEN_ERROR,
    TRAZO_TOKEN_IDENTIFIER,
    TRAZO_TOKEN_INTEGER,
    TRAZO_TOKEN_FLOAT,
    TRAZO_TOKEN_STRING,
    TRAZO_TOKEN_CHARACTER,
    TRAZO_TOKEN_KW_BREAK,
    TRAZO_TOKEN_KW_CASE,
    TRAZO_TOKEN_KW_CHAR,
    TRAZO_TOKEN_KW_CONST,
    TRAZO_TOKEN_KW_CONTINUE,
    TRAZO_TOKEN_KW_DEFAULT,
    TRAZO_TOKEN_KW_ELSE,
    TRAZO_TOKEN_KW_ENUM,
    TRAZO_TOKEN_KW_EXTERN,
    TRAZO_TOKEN_KW_FLOAT,
    TRAZO_TOKEN_KW_FOR,
    TRAZO_TOKEN_KW_IF,
    TRAZO_TOKEN_KW_INT,
    TRAZO_TOKEN_KW_RETURN,
    TRAZO_TOKEN_KW_STATIC,
    TRAZO_TOKEN_KW_STRUCT,
    TRAZO_TOKEN_KW_SWITCH,
    TRAZO_TOKEN_KW_TYPEDEF,
    TRAZO_TOKEN_KW_UNION,
    TRAZO_TOKEN_KW_VOID,
    TRAZO_TOKEN_KW_VOLATILE,
    TRAZO_TOKEN_KW_WHILE,
    TRAZO_TOKEN_KW_FUNC,
    TRAZO_TOKEN_KW_UNSAFE,
    TRAZO_TOKEN_KW_EXPORT,
    TRAZO_TOKEN_KW_IMPORT,
    TRAZO_TOKEN_KW_FROM,
    TRAZO_TOKEN_KW_AS,
    TRAZO_TOKEN_KW_IMPORT_C,
    TRAZO_TOKEN_LEFT_PAREN,
    TRAZO_TOKEN_RIGHT_PAREN,
    TRAZO_TOKEN_LEFT_BRACE,
    TRAZO_TOKEN_RIGHT_BRACE,
    TRAZO_TOKEN_LEFT_BRACKET,
    TRAZO_TOKEN_RIGHT_BRACKET,
    TRAZO_TOKEN_COMMA,
    TRAZO_TOKEN_SEMICOLON,
    TRAZO_TOKEN_COLON,
    TRAZO_TOKEN_DOT,
    TRAZO_TOKEN_ARROW,
    TRAZO_TOKEN_PLUS,
    TRAZO_TOKEN_PLUS_PLUS,
    TRAZO_TOKEN_MINUS,
    TRAZO_TOKEN_MINUS_MINUS,
    TRAZO_TOKEN_STAR,
    TRAZO_TOKEN_SLASH,
    TRAZO_TOKEN_PERCENT,
    TRAZO_TOKEN_AMPERSAND,
    TRAZO_TOKEN_PIPE,
    TRAZO_TOKEN_CARET,
    TRAZO_TOKEN_TILDE,
    TRAZO_TOKEN_BANG,
    TRAZO_TOKEN_EQUAL,
    TRAZO_TOKEN_EQUAL_EQUAL,
    TRAZO_TOKEN_BANG_EQUAL,
    TRAZO_TOKEN_LESS,
    TRAZO_TOKEN_LESS_EQUAL,
    TRAZO_TOKEN_GREATER,
    TRAZO_TOKEN_GREATER_EQUAL,
    TRAZO_TOKEN_AND_AND,
    TRAZO_TOKEN_OR_OR
} TrazoTokenKind;

typedef struct {
    TrazoTokenKind kind;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
} TrazoToken;

typedef struct {
    TrazoToken *items;
    size_t count;
    size_t capacity;
} TrazoTokenList;

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} TrazoLexerError;

typedef enum {
    TRAZO_AST_PROGRAM,
    TRAZO_AST_IMPORT_C,
    TRAZO_AST_IMPORT_MODULE,
    TRAZO_AST_FROM_IMPORT,
    TRAZO_AST_FUNCTION,
    TRAZO_AST_BLOCK,
    TRAZO_AST_IF,
    TRAZO_AST_WHILE,
    TRAZO_AST_FOR,
    TRAZO_AST_SWITCH,
    TRAZO_AST_CASE,
    TRAZO_AST_DEFAULT,
    TRAZO_AST_BREAK,
    TRAZO_AST_CONTINUE,
    TRAZO_AST_UNSAFE,
    TRAZO_AST_UNARY,
    TRAZO_AST_CALL,
    TRAZO_AST_BINARY,
    TRAZO_AST_RETURN,
    TRAZO_AST_VAR_DECL,
    TRAZO_AST_ENUM_DECL,
    TRAZO_AST_TYPEDEF,
    TRAZO_AST_STRUCT_DECL,
    TRAZO_AST_UNION_DECL,
    TRAZO_AST_IDENTIFIER,
    TRAZO_AST_INTEGER,
    TRAZO_AST_FLOAT,
    TRAZO_AST_STRING,
    TRAZO_AST_CHARACTER,
    TRAZO_AST_SIZEOF,
    TRAZO_AST_CAST
} TrazoAstKind;

#define TRAZO_AST_FLAG_PREFIX 1u
#define TRAZO_AST_FLAG_CONST 2u
#define TRAZO_AST_FLAG_STATIC 4u
#define TRAZO_AST_FLAG_VOLATILE 8u
#define TRAZO_AST_FLAG_EXTERN_C 16u
#define TRAZO_AST_FLAG_DECLARATION 32u
#define TRAZO_AST_FLAG_GROUPED 64u
#define TRAZO_AST_FLAG_POINTER 128u
#define TRAZO_AST_FLAG_ARRAY 64u
#define TRAZO_AST_FLAG_EXPORT 256u

typedef struct {
    TrazoAstKind *kind;
    unsigned int *flags;
    size_t *first_child;
    size_t *child_count;
    size_t *token_index;
    size_t *resolved_function;
    size_t *children;
    size_t node_count;
    size_t node_capacity;
    size_t child_count_total;
    size_t child_capacity;
} TrazoAst;

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} TrazoParserError;

typedef struct {
    const char *start;
    size_t length;
    size_t line;
    size_t column;
} TrazoCImport;

typedef struct {
    TrazoCImport *items;
    size_t count;
    size_t capacity;
} TrazoCImportList;

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} TrazoPreprocessorError;

typedef enum {
    TRAZO_MODULE_SOURCE,
    TRAZO_MODULE_C_HEADER,
    TRAZO_MODULE_C_SOURCE
} TrazoModuleKind;

typedef struct {
    TrazoModuleKind kind;
    const char *module;
    size_t module_length;
    const char *symbol;
    size_t symbol_length;
    const char *alias;
    size_t alias_length;
    size_t line;
    size_t column;
} TrazoModuleImport;

typedef struct {
    TrazoModuleImport *items;
    size_t count;
    size_t capacity;
} TrazoModuleImportList;

typedef struct {
    char *source;
    size_t length;
} TrazoPreprocessedSource;

typedef struct {
    char *source;
    size_t length;
} TrazoGeneratedC;

typedef enum {
    TRAZO_TYPE_INVALID,
    TRAZO_TYPE_VOID,
    TRAZO_TYPE_INT,
    TRAZO_TYPE_FLOAT,
    TRAZO_TYPE_CHAR
} TrazoTypeKind;

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} TrazoTypecheckError;

int trazo_lex(const char *source, TrazoTokenList *tokens, TrazoLexerError *error);
void trazo_tokens_free(TrazoTokenList *tokens);
const char *trazo_token_name(TrazoTokenKind kind);
int trazo_preprocess(const TrazoTokenList *tokens, TrazoCImportList *imports,
                     TrazoPreprocessorError *error);
void trazo_c_imports_free(TrazoCImportList *imports);
int trazo_preprocess_source(const char *source, TrazoPreprocessedSource *output,
                            TrazoPreprocessorError *error);
void trazo_preprocessed_source_free(TrazoPreprocessedSource *output);
int trazo_collect_ffi_import(const TrazoTokenList *tokens, size_t index,
                            TrazoModuleImportList *modules,
                            TrazoPreprocessorError *error, size_t *next_index);
int trazo_collect_modules(const TrazoTokenList *tokens, TrazoModuleImportList *modules,
                          TrazoTokenList *code_tokens, TrazoPreprocessorError *error);
void trazo_modules_free(TrazoModuleImportList *modules);
int trazo_parse(const TrazoTokenList *tokens, TrazoAst *ast, TrazoParserError *error);
void trazo_ast_free(TrazoAst *ast);
const char *trazo_ast_name(TrazoAstKind kind);
char *trazo_mangle_function(const TrazoTokenList *tokens, const TrazoAst *ast,
                            size_t function);
int trazo_codegen_c(const TrazoTokenList *tokens, const TrazoAst *ast,
                    const TrazoModuleImportList *modules, const char *header_path,
                    TrazoGeneratedC *output,
                    TrazoParserError *error);
int trazo_codegen_h(const TrazoTokenList *tokens, const TrazoAst *ast,
                    const char *header_path, TrazoGeneratedC *output,
                    TrazoParserError *error);
void trazo_generated_c_free(TrazoGeneratedC *output);
int trazo_typecheck(const TrazoTokenList *tokens, const TrazoAst *ast,
                    TrazoTypecheckError *error);
const char *trazo_type_name(TrazoTypeKind kind);

#endif
