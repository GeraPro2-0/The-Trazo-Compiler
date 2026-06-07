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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int line;
    int column;
    int token_column;
} Lexer;

typedef struct {
    const char* english;
    const char* spanish;
    TrazoTokenType type;
} Keyword;

static const Keyword KEYWORDS[] = {
    {"int", "entero", TRAZO_TOKEN_TYPE_INT},
    {"string", "cadena", TRAZO_TOKEN_TYPE_STRING},
    {"float", "flotante", TRAZO_TOKEN_TYPE_FLOAT},
    {"bool", "booleano", TRAZO_TOKEN_TYPE_BOOL},
    {"byte", "byte", TRAZO_TOKEN_TYPE_BYTE},
    {"char", "caracter", TRAZO_TOKEN_TYPE_CHAR},
    {"rune", "runa", TRAZO_TOKEN_TYPE_RUNE},
    {"double", "doble", TRAZO_TOKEN_TYPE_DOUBLE},
    {"void", "vacio", TRAZO_TOKEN_TYPE_VOID},
    {"i8", "e8", TRAZO_TOKEN_TYPE_I8},
    {"i16", "e16", TRAZO_TOKEN_TYPE_I16},
    {"i32", "e32", TRAZO_TOKEN_TYPE_I32},
    {"i64", "e64", TRAZO_TOKEN_TYPE_I64},
    {"u8", "u8", TRAZO_TOKEN_TYPE_U8},
    {"u16", "u16", TRAZO_TOKEN_TYPE_U16},
    {"u32", "u32", TRAZO_TOKEN_TYPE_U32},
    {"u64", "u64", TRAZO_TOKEN_TYPE_U64},
    {"f32", "f32", TRAZO_TOKEN_TYPE_F32},
    {"f64", "f64", TRAZO_TOKEN_TYPE_F64},
    {"decimal", "decimal", TRAZO_TOKEN_TYPE_DECIMAL},
    {"dec32", "dec32", TRAZO_TOKEN_TYPE_DEC32},
    {"dec64", "dec64", TRAZO_TOKEN_TYPE_DEC64},
    {"dec128", "dec128", TRAZO_TOKEN_TYPE_DEC128},
    {"ptr", "ptr", TRAZO_TOKEN_TYPE_PTR},

    {"if", "si", TRAZO_TOKEN_IF},
    {"elif", "sino_si", TRAZO_TOKEN_ELIF},
    {"else", "sino", TRAZO_TOKEN_ELSE},
    {"while", "mientras", TRAZO_TOKEN_WHILE},
    {"for", "para", TRAZO_TOKEN_FOR},
    {"struct", "estructura", TRAZO_TOKEN_STRUCT},
    {"func", "funcion", TRAZO_TOKEN_FUNC},
    {"extern", "externo", TRAZO_TOKEN_EXTERN},
    {"return", "retornar", TRAZO_TOKEN_RETURN},
    {"break", "romper", TRAZO_TOKEN_BREAK},
    {"continue", "continuar", TRAZO_TOKEN_CONTINUE},

    {"malloc", "reservar", TRAZO_TOKEN_MALLOC},
    {"alloc", "asignar_mem", TRAZO_TOKEN_ALLOC},
    {"calloc", "asignar_cero", TRAZO_TOKEN_CALLOC},
    {"realloc", "reasignar", TRAZO_TOKEN_REALLOC},
    {"free", "liberar", TRAZO_TOKEN_FREE},
    {"delete", "eliminar", TRAZO_TOKEN_DELETE},
    {"sizeof", "tamano_de", TRAZO_TOKEN_SIZEOF},
    {"print", "imprimir", TRAZO_TOKEN_PRINT},
    {"read_line", "leer_linea", TRAZO_TOKEN_READ_LINE},
    {"read_file", "leer_archivo", TRAZO_TOKEN_READ_FILE},
    {"write_file", "escribir_archivo", TRAZO_TOKEN_WRITE_FILE},
    {"append_file", "anexar_archivo", TRAZO_TOKEN_APPEND_FILE},
    {"file_exists", "archivo_existe", TRAZO_TOKEN_FILE_EXISTS},
    {"len", "longitud", TRAZO_TOKEN_LEN},
    {"memcpy", "copimem", TRAZO_TOKEN_MEMCPY},
    {"memset", "llenmem", TRAZO_TOKEN_MEMSET},
    {"memcmp", "comparmem", TRAZO_TOKEN_MEMCMP},
    {"volatile", "volatil", TRAZO_TOKEN_VOLATILE},
    {"as", "como", TRAZO_TOKEN_AS},

    {"true", "verdadero", TRAZO_TOKEN_TRUE},
    {"false", "falso", TRAZO_TOKEN_FALSE},
    {"null", "nulo", TRAZO_TOKEN_NULL},
};

static int is_at_end(const Lexer* lexer) {
    return *lexer->current == '\0';
}

static char peek(const Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(const Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static char advance(Lexer* lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static int match(Lexer* lexer, char expected) {
    if (is_at_end(lexer) || *lexer->current != expected) return 0;
    advance(lexer);
    return 1;
}

static int token_list_push(TrazoTokenList* list, TrazoToken token) {
    if (list->count == list->capacity) {
        size_t next_capacity = list->capacity < 16 ? 16 : list->capacity * 2;
        TrazoToken* next_items = (TrazoToken*)trazo_realloc(list->items, next_capacity * sizeof(TrazoToken));
        if (!next_items) return 0;
        list->items = next_items;
        list->capacity = next_capacity;
    }

    list->items[list->count++] = token;
    return 1;
}

static TrazoToken make_token(const Lexer* lexer, TrazoTokenType type) {
    TrazoToken token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->token_column;
    return token;
}

static TrazoToken error_token(const Lexer* lexer) {
    return make_token(lexer, TRAZO_TOKEN_ERROR);
}

static int keyword_matches(const char* start, size_t length, const char* text) {
    return strlen(text) == length && memcmp(start, text, length) == 0;
}

static TrazoTokenType identifier_type(const char* start, size_t length) {
    size_t count = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);

    for (size_t i = 0; i < count; i++) {
        if (keyword_matches(start, length, KEYWORDS[i].english)) return KEYWORDS[i].type;
        if (keyword_matches(start, length, KEYWORDS[i].spanish)) return KEYWORDS[i].type;
    }

    return TRAZO_TOKEN_IDENTIFIER;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_part(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int text_equals(const char* start, size_t length, const char* text) {
    return strlen(text) == length && memcmp(start, text, length) == 0;
}

static TrazoToken scan_identifier(Lexer* lexer) {
    while (is_ident_part(peek(lexer))) {
        advance(lexer);
    }

    size_t length = (size_t)(lexer->current - lexer->start);

    if ((text_equals(lexer->start, length, "unsafe") ||
         text_equals(lexer->start, length, "inseguro")) &&
        peek(lexer) == '.') {
        const char* saved_current = lexer->current;
        int saved_column = lexer->column;

        advance(lexer);
        const char* mode_start = lexer->current;
        while (is_ident_part(peek(lexer))) advance(lexer);

        size_t mode_length = (size_t)(lexer->current - mode_start);
        if (text_equals(mode_start, mode_length, "MAX")) {
            return make_token(lexer, TRAZO_TOKEN_UNSAFE_MAX);
        }

        lexer->current = saved_current;
        lexer->column = saved_column;
    }

    return make_token(lexer, identifier_type(lexer->start, length));
}

static TrazoToken scan_number(Lexer* lexer) {
    TrazoTokenType type = TRAZO_TOKEN_INT_LITERAL;

    if (lexer->start[0] == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        advance(lexer);
        while (isxdigit((unsigned char)peek(lexer))) advance(lexer);
        return make_token(lexer, TRAZO_TOKEN_INT_LITERAL);
    }

    if (lexer->start[0] == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B')) {
        advance(lexer);
        while (peek(lexer) == '0' || peek(lexer) == '1') advance(lexer);
        return make_token(lexer, TRAZO_TOKEN_INT_LITERAL);
    }

    while (isdigit((unsigned char)peek(lexer))) advance(lexer);

    if (peek(lexer) == '.' && isdigit((unsigned char)peek_next(lexer))) {
        type = TRAZO_TOKEN_FLOAT_LITERAL;
        advance(lexer);
        while (isdigit((unsigned char)peek(lexer))) advance(lexer);
    }

    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        type = TRAZO_TOKEN_FLOAT_LITERAL;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') advance(lexer);
        while (isdigit((unsigned char)peek(lexer))) advance(lexer);
    }

    if (peek(lexer) == 'f' || peek(lexer) == 'F') {
        type = TRAZO_TOKEN_FLOAT_LITERAL;
        advance(lexer);
    }

    return make_token(lexer, type);
}

static TrazoToken scan_string(Lexer* lexer) {
    while (!is_at_end(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\\') {
            advance(lexer);
            if (!is_at_end(lexer)) advance(lexer);
        } else {
            advance(lexer);
        }
    }

    if (is_at_end(lexer)) return error_token(lexer);
    advance(lexer);
    return make_token(lexer, TRAZO_TOKEN_STRING_LITERAL);
}

static TrazoToken scan_char(Lexer* lexer) {
    if (is_at_end(lexer) || peek(lexer) == '\n') return error_token(lexer);

    while (!is_at_end(lexer) && peek(lexer) != '\'' && peek(lexer) != '\n') {
        if (peek(lexer) == '\\') {
            advance(lexer);
            if (!is_at_end(lexer)) advance(lexer);
        } else {
            advance(lexer);
        }
    }

    if (!match(lexer, '\'')) return error_token(lexer);
    return make_token(lexer, TRAZO_TOKEN_CHAR_LITERAL);
}

static void skip_line_comment(Lexer* lexer) {
    while (!is_at_end(lexer) && peek(lexer) != '\n') advance(lexer);
}

static int skip_block_comment(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        if (peek(lexer) == '*' && peek_next(lexer) == '/') {
            advance(lexer);
            advance(lexer);
            return 1;
        }
        advance(lexer);
    }

    return 0;
}

static int skip_whitespace_and_comments(Lexer* lexer, TrazoTokenList* tokens) {
    (void)tokens;

    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                advance(lexer);
                break;
            case '#':
                if (peek_next(lexer) == '#') {
                    advance(lexer);
                    advance(lexer);
                    if (peek(lexer) == '#') advance(lexer);
                    while (!is_at_end(lexer)) {
                        if (peek(lexer) == '#' && peek_next(lexer) == '#' && lexer->current[2] == '#') {
                            advance(lexer);
                            advance(lexer);
                            advance(lexer);
                            break;
                        }
                        advance(lexer);
                    }
                } else {
                    skip_line_comment(lexer);
                }
                break;
            case '/':
                if (peek_next(lexer) == '/') {
                    advance(lexer);
                    advance(lexer);
                    skip_line_comment(lexer);
                    break;
                }
                if (peek_next(lexer) == '*') {
                    advance(lexer);
                    advance(lexer);
                    if (!skip_block_comment(lexer)) return 0;
                    break;
                }
                return 1;
            default:
                return 1;
        }
    }
}

static TrazoToken scan_token(Lexer* lexer) {
    lexer->start = lexer->current;
    lexer->token_column = lexer->column;

    if (is_at_end(lexer)) return make_token(lexer, TRAZO_TOKEN_EOF);

    char c = advance(lexer);

    if (is_ident_start(c)) return scan_identifier(lexer);
    if (isdigit((unsigned char)c)) return scan_number(lexer);

    switch (c) {
        case '"': return scan_string(lexer);
        case '\'': return scan_char(lexer);
        case '+': return make_token(lexer, TRAZO_TOKEN_PLUS);
        case '-': return make_token(lexer, match(lexer, '>') ? TRAZO_TOKEN_ARROW : TRAZO_TOKEN_MINUS);
        case '*': return make_token(lexer, TRAZO_TOKEN_STAR);
        case '/': return make_token(lexer, TRAZO_TOKEN_SLASH);
        case '%': return make_token(lexer, TRAZO_TOKEN_PERCENT);
        case '=': return make_token(lexer, match(lexer, '=') ? TRAZO_TOKEN_EQUAL_EQUAL : TRAZO_TOKEN_EQUAL);
        case '!': return make_token(lexer, match(lexer, '=') ? TRAZO_TOKEN_BANG_EQUAL : TRAZO_TOKEN_BANG);
        case '<':
            if (match(lexer, '=')) return make_token(lexer, TRAZO_TOKEN_LESS_EQUAL);
            if (match(lexer, '<')) return make_token(lexer, TRAZO_TOKEN_LESS_LESS);
            return make_token(lexer, TRAZO_TOKEN_LESS);
        case '>':
            if (match(lexer, '=')) return make_token(lexer, TRAZO_TOKEN_GREATER_EQUAL);
            if (match(lexer, '>')) return make_token(lexer, TRAZO_TOKEN_GREATER_GREATER);
            return make_token(lexer, TRAZO_TOKEN_GREATER);
        case '&': return make_token(lexer, match(lexer, '&') ? TRAZO_TOKEN_AMP_AMP : TRAZO_TOKEN_AMPERSAND);
        case '|': return make_token(lexer, match(lexer, '|') ? TRAZO_TOKEN_PIPE_PIPE : TRAZO_TOKEN_PIPE);
        case '^': return make_token(lexer, TRAZO_TOKEN_CARET);
        case '~': return make_token(lexer, TRAZO_TOKEN_TILDE);
        case '@': return make_token(lexer, TRAZO_TOKEN_AT);
        case '(': return make_token(lexer, TRAZO_TOKEN_LEFT_PAREN);
        case ')': return make_token(lexer, TRAZO_TOKEN_RIGHT_PAREN);
        case '{': return make_token(lexer, TRAZO_TOKEN_LEFT_BRACE);
        case '}': return make_token(lexer, TRAZO_TOKEN_RIGHT_BRACE);
        case '[': return make_token(lexer, TRAZO_TOKEN_LEFT_BRACKET);
        case ']': return make_token(lexer, TRAZO_TOKEN_RIGHT_BRACKET);
        case ',': return make_token(lexer, TRAZO_TOKEN_COMMA);
        case ';': return make_token(lexer, TRAZO_TOKEN_SEMICOLON);
        case ':': return make_token(lexer, TRAZO_TOKEN_COLON);
        case '.': return make_token(lexer, TRAZO_TOKEN_DOT);
        default: return make_token(lexer, TRAZO_TOKEN_ERROR);
    }
}

TrazoTokenList trazo_lex_source(const char* source) {
    Lexer lexer;
    TrazoTokenList tokens = {0};

    lexer.source = source ? source : "";
    lexer.start = lexer.source;
    lexer.current = lexer.source;
    lexer.line = 1;
    lexer.column = 1;
    lexer.token_column = 1;

    for (;;) {
        if (!skip_whitespace_and_comments(&lexer, &tokens)) {
            lexer.start = lexer.current;
            lexer.token_column = lexer.column;
            token_list_push(&tokens, error_token(&lexer));
            break;
        }

        TrazoToken token = scan_token(&lexer);
        if (!token_list_push(&tokens, token)) {
            trazo_free_tokens(tokens);
            return (TrazoTokenList){0};
        }

        if (token.type == TRAZO_TOKEN_EOF || token.type == TRAZO_TOKEN_ERROR) {
            break;
        }
    }

    return tokens;
}

void trazo_free_tokens(TrazoTokenList tokens) {
    trazo_free(tokens.items);
}
