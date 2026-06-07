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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char* exe) {
    printf("Trazo\n");
    printf("Usage:\n");
    printf("  %s <file.trz>        Parse a Trazo source file (.trz)\n", exe);
    printf("  %s --tokens <file>   Print lexer tokens\n", exe);
    printf("  %s --help            Show this help\n", exe);
    printf("---\n");
    printf("Trazo\n");
    printf("Uso:\n");
    printf("  %s <file.trz>        Analizar un archivo fuente de Trazo (.trz)\n", exe);
    printf("  %s --tokens <file>   Imprimir tokens del analizador léxico\n", exe);
    printf("  %s --help            Mostrar esta ayuda\n", exe);
}

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    char* data;
    long size;
    size_t read_count;

    if (!file) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    rewind(file);

    data = (char*)trazo_alloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    read_count = fread(data, 1, (size_t)size, file);
    data[read_count] = '\0';
    fclose(file);
    return data;
}

static const char* token_name(TrazoTokenType type) {
    switch (type) {
        case TRAZO_TOKEN_EOF: return "EOF";
        case TRAZO_TOKEN_ERROR: return "ERROR";
        case TRAZO_TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TRAZO_TOKEN_INT_LITERAL: return "INT_LITERAL";
        case TRAZO_TOKEN_FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TRAZO_TOKEN_STRING_LITERAL: return "STRING_LITERAL";
        case TRAZO_TOKEN_CHAR_LITERAL: return "CHAR_LITERAL";
        case TRAZO_TOKEN_TRUE: return "TRUE";
        case TRAZO_TOKEN_FALSE: return "FALSE";
        case TRAZO_TOKEN_TYPE_INT: return "TYPE_INT";
        case TRAZO_TOKEN_TYPE_I8: return "TYPE_I8";
        case TRAZO_TOKEN_TYPE_I16: return "TYPE_I16";
        case TRAZO_TOKEN_TYPE_I32: return "TYPE_I32";
        case TRAZO_TOKEN_TYPE_I64: return "TYPE_I64";
        case TRAZO_TOKEN_TYPE_U8: return "TYPE_U8";
        case TRAZO_TOKEN_TYPE_U16: return "TYPE_U16";
        case TRAZO_TOKEN_TYPE_U32: return "TYPE_U32";
        case TRAZO_TOKEN_TYPE_U64: return "TYPE_U64";
        case TRAZO_TOKEN_TYPE_BYTE: return "TYPE_BYTE";
        case TRAZO_TOKEN_TYPE_CHAR: return "TYPE_CHAR";
        case TRAZO_TOKEN_TYPE_RUNE: return "TYPE_RUNE";
        case TRAZO_TOKEN_TYPE_F32: return "TYPE_F32";
        case TRAZO_TOKEN_TYPE_F64: return "TYPE_F64";
        case TRAZO_TOKEN_TYPE_DECIMAL: return "TYPE_DECIMAL";
        case TRAZO_TOKEN_TYPE_DEC32: return "TYPE_DEC32";
        case TRAZO_TOKEN_TYPE_DEC64: return "TYPE_DEC64";
        case TRAZO_TOKEN_TYPE_DEC128: return "TYPE_DEC128";
        case TRAZO_TOKEN_TYPE_PTR: return "TYPE_PTR";
        case TRAZO_TOKEN_TYPE_FLOAT: return "TYPE_FLOAT";
        case TRAZO_TOKEN_TYPE_DOUBLE: return "TYPE_DOUBLE";
        case TRAZO_TOKEN_TYPE_BOOL: return "TYPE_BOOL";
        case TRAZO_TOKEN_TYPE_STRING: return "TYPE_STRING";
        case TRAZO_TOKEN_TYPE_VOID: return "TYPE_VOID";
        case TRAZO_TOKEN_IF: return "IF";
        case TRAZO_TOKEN_ELIF: return "ELIF";
        case TRAZO_TOKEN_ELSE: return "ELSE";
        case TRAZO_TOKEN_WHILE: return "WHILE";
        case TRAZO_TOKEN_FOR: return "FOR";
        case TRAZO_TOKEN_STRUCT: return "STRUCT";
        case TRAZO_TOKEN_FUNC: return "FUNC";
        case TRAZO_TOKEN_EXTERN: return "EXTERN";
        case TRAZO_TOKEN_RETURN: return "RETURN";
        case TRAZO_TOKEN_BREAK: return "BREAK";
        case TRAZO_TOKEN_CONTINUE: return "CONTINUE";
        case TRAZO_TOKEN_UNSAFE_MAX: return "UNSAFE_MAX";
        case TRAZO_TOKEN_MALLOC: return "MALLOC";
        case TRAZO_TOKEN_ALLOC: return "ALLOC";
        case TRAZO_TOKEN_CALLOC: return "CALLOC";
        case TRAZO_TOKEN_REALLOC: return "REALLOC";
        case TRAZO_TOKEN_FREE: return "FREE";
        case TRAZO_TOKEN_DELETE: return "DELETE";
        case TRAZO_TOKEN_SIZEOF: return "SIZEOF";
        case TRAZO_TOKEN_PRINT: return "PRINT";
        case TRAZO_TOKEN_READ_LINE: return "READ_LINE";
        case TRAZO_TOKEN_READ_FILE: return "READ_FILE";
        case TRAZO_TOKEN_WRITE_FILE: return "WRITE_FILE";
        case TRAZO_TOKEN_APPEND_FILE: return "APPEND_FILE";
        case TRAZO_TOKEN_FILE_EXISTS: return "FILE_EXISTS";
        case TRAZO_TOKEN_LEN: return "LEN";
        case TRAZO_TOKEN_MEMCPY: return "MEMCPY";
        case TRAZO_TOKEN_MEMSET: return "MEMSET";
        case TRAZO_TOKEN_MEMCMP: return "MEMCMP";
        case TRAZO_TOKEN_VOLATILE: return "VOLATILE";
        case TRAZO_TOKEN_AS: return "AS";
        case TRAZO_TOKEN_NULL: return "NULL";
        case TRAZO_TOKEN_PLUS: return "PLUS";
        case TRAZO_TOKEN_MINUS: return "MINUS";
        case TRAZO_TOKEN_STAR: return "STAR";
        case TRAZO_TOKEN_SLASH: return "SLASH";
        case TRAZO_TOKEN_PERCENT: return "PERCENT";
        case TRAZO_TOKEN_EQUAL: return "EQUAL";
        case TRAZO_TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TRAZO_TOKEN_BANG_EQUAL: return "BANG_EQUAL";
        case TRAZO_TOKEN_LESS: return "LESS";
        case TRAZO_TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TRAZO_TOKEN_GREATER: return "GREATER";
        case TRAZO_TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TRAZO_TOKEN_AMP_AMP: return "AMP_AMP";
        case TRAZO_TOKEN_PIPE_PIPE: return "PIPE_PIPE";
        case TRAZO_TOKEN_BANG: return "BANG";
        case TRAZO_TOKEN_AMPERSAND: return "AMPERSAND";
        case TRAZO_TOKEN_PIPE: return "PIPE";
        case TRAZO_TOKEN_CARET: return "CARET";
        case TRAZO_TOKEN_TILDE: return "TILDE";
        case TRAZO_TOKEN_LESS_LESS: return "LESS_LESS";
        case TRAZO_TOKEN_GREATER_GREATER: return "GREATER_GREATER";
        case TRAZO_TOKEN_ARROW: return "ARROW";
        case TRAZO_TOKEN_AT: return "AT";
        case TRAZO_TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TRAZO_TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TRAZO_TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TRAZO_TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TRAZO_TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TRAZO_TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TRAZO_TOKEN_COMMA: return "COMMA";
        case TRAZO_TOKEN_SEMICOLON: return "SEMICOLON";
        case TRAZO_TOKEN_COLON: return "COLON";
        case TRAZO_TOKEN_DOT: return "DOT";
    }

    return "UNKNOWN";
}

static int has_lex_error(TrazoTokenList tokens) {
    for (size_t i = 0; i < tokens.count; i++) {
        if (tokens.items[i].type == TRAZO_TOKEN_ERROR) {
            fprintf(stderr, "Lex error at line %d, column %d near '%.*s'\n",
                    tokens.items[i].line,
                    tokens.items[i].column,
                    (int)tokens.items[i].length,
                    tokens.items[i].start);
            return 1;
        }
    }

    return 0;
}

static void print_tokens(TrazoTokenList tokens) {
    for (size_t i = 0; i < tokens.count; i++) {
        TrazoToken token = tokens.items[i];
        printf("%4d:%-4d %-18s %.*s\n",
               token.line,
               token.column,
               token_name(token.type),
               (int)token.length,
               token.start);
    }
}

int main(int argc, char** argv) {
    const char* path;
    int show_tokens = 0;
    char* source;
    TrazoTokenList tokens;
    TrazoParseError error;
    TrazoProgram* program;
    int result;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (!trazo_memory_init()) {
        fprintf(stderr, "Error: memory initialization failed\n");
        fprintf(stderr, "Error: Falló la inicialización de la memoria.\n");
        return 1;
    }

    if (!trazo_runtime_init()) {
        fprintf(stderr, "Error: runtime initialization failed\n");
        fprintf(stderr, "Error: Falló la inicialización del tiempo de ejecución.\n");
        trazo_memory_shutdown();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        trazo_runtime_destroy();
        trazo_memory_shutdown();
        return 0;
    }

    if (strcmp(argv[1], "--tokens") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --tokens requires a file path\n");
            fprintf(stderr, "Error: --tokens requiere una ruta de archivo\n");
            trazo_runtime_destroy();
            trazo_memory_shutdown();
            return 1;
        }
        show_tokens = 1;
        path = argv[2];
    } else {
        path = argv[1];
    }

    source = read_file(path);
    if (!source) {
        fprintf(stderr, "Error: could not read '%s'\n", path);
        fprintf(stderr, "Error: No se pudo leer '%s'\n", path);
        trazo_runtime_destroy();
        trazo_memory_shutdown();
        return 1;
    }

    tokens = trazo_lex_source(source);
    if (!tokens.items || has_lex_error(tokens)) {
        fprintf(stderr, "Error: lexical analysis failed\n");
        fprintf(stderr, "Error: Falló el análisis léxico.\n");
        trazo_free_tokens(tokens);
        trazo_free(source);
        return 1;
    }

    if (show_tokens) {
        print_tokens(tokens);
        trazo_free_tokens(tokens);
        trazo_free(source);
        trazo_runtime_destroy();
        trazo_memory_shutdown();
        return 0;
    }

    program = trazo_parse_tokens(tokens.items, tokens.count, &error);
    if (!program) {
        fprintf(stderr, "Parse error: %s\n", error.message[0] ? error.message : "unknown error");
        fprintf(stderr, "Error de parseo: %s\n", error.message[0] ? error.message : "error desconocido");
        trazo_free_tokens(tokens);
        trazo_free(source);
        trazo_runtime_destroy();
        trazo_memory_shutdown();
        return 1;
    }

    result = trazo_execute(program);

    trazo_free_program(program);
    trazo_free_tokens(tokens);
    trazo_free(source);
    trazo_runtime_destroy();
    trazo_memory_shutdown();
    return result;
}
