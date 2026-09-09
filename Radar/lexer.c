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

#include "radar.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define RADAR_TOKEN_INITIAL_CAPACITY 64

typedef struct {
    const char *current;
    size_t line;
    size_t column;
    RadarTokenList *tokens;
} Lexer;

static void set_error(RadarLexerError *error, const char *message,
                      size_t line, size_t column) {
    if (error) {
        error->message = message;
        error->line = line;
        error->column = column;
    }
}

static int append_token(Lexer *lexer, RadarTokenKind kind, const char *start,
                        size_t line, size_t column) {
    RadarToken *items;
    size_t capacity;
    if (lexer->tokens->count == lexer->tokens->capacity) {
        capacity = lexer->tokens->capacity == 0
            ? RADAR_TOKEN_INITIAL_CAPACITY
            : lexer->tokens->capacity * 2;
        items = realloc(lexer->tokens->items, capacity * sizeof(*items));
        if (!items) return 0;
        lexer->tokens->items = items;
        lexer->tokens->capacity = capacity;
    }
    items = lexer->tokens->items;
    items[lexer->tokens->count++] = (RadarToken){
        kind, start, (size_t)(lexer->current - start), line, column
    };
    return 1;
}

static int is_identifier_start(char character) {
    return isalpha((unsigned char)character) || character == '_';
}

static int is_identifier_part(char character) {
    return isalnum((unsigned char)character) || character == '_' || character == '-';
}

static RadarTokenKind literal_kind(const char *start, size_t length) {
    if (length == 4 && strncmp(start, "true", length) == 0) return RADAR_TOKEN_TRUE;
    if (length == 5 && strncmp(start, "false", length) == 0) return RADAR_TOKEN_FALSE;
    return RADAR_TOKEN_IDENTIFIER;
}

static RadarTokenKind punctuation_kind(char character) {
    switch (character) {
        case '[': return RADAR_TOKEN_LEFT_BRACKET;
        case ']': return RADAR_TOKEN_RIGHT_BRACKET;
        case '{': return RADAR_TOKEN_LEFT_BRACE;
        case '}': return RADAR_TOKEN_RIGHT_BRACE;
        case ',': return RADAR_TOKEN_COMMA;
        case '.': return RADAR_TOKEN_DOT;
        case '=': return RADAR_TOKEN_EQUAL;
        default: return RADAR_TOKEN_ERROR;
    }
}

static RadarTokenKind scan_punctuation(Lexer *lexer) {
    const char *current = lexer->current;
    RadarTokenKind kind = punctuation_kind(current[0]);
    if (current[0] == '[' && current[1] == '[') kind = RADAR_TOKEN_DOUBLE_LEFT_BRACKET;
    else if (current[0] == ']' && current[1] == ']') kind = RADAR_TOKEN_DOUBLE_RIGHT_BRACKET;
    else if (kind == RADAR_TOKEN_ERROR) return RADAR_TOKEN_ERROR;
    lexer->current += (kind == RADAR_TOKEN_DOUBLE_LEFT_BRACKET
                       || kind == RADAR_TOKEN_DOUBLE_RIGHT_BRACKET) ? 2 : 1;
    lexer->column += (size_t)(lexer->current - current);
    return kind;
}

static int is_hex_digit(char character) {
    return isdigit((unsigned char)character)
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}

static int valid_escape(const char *escape) {
    size_t index;
    if (strchr("btnfr\\\"", escape[0])) return 1;
    if (escape[0] != 'u' && escape[0] != 'U') return 0;
    for (index = 1; index < (escape[0] == 'u' ? 5u : 9u); ++index) {
        if (escape[index] == '\0' || !is_hex_digit(escape[index])) return 0;
    }
    return 1;
}

static int scan_quoted_string(Lexer *lexer, char delimiter, RadarTokenKind kind) {
    const char *start = lexer->current;
    size_t line = lexer->line;
    size_t column = lexer->column;
    int multiline = lexer->current[0] == delimiter
        && lexer->current[1] == delimiter && lexer->current[2] == delimiter;
    if (multiline && delimiter == '"') kind = RADAR_TOKEN_MULTILINE_STRING;
    if (multiline && delimiter == '\'') kind = RADAR_TOKEN_MULTILINE_LITERAL_STRING;
    if (multiline) {
        lexer->current += 3;
        lexer->column += 3;
    } else {
        ++lexer->current;
        ++lexer->column;
    }
    while (*lexer->current) {
        if (!multiline && *lexer->current == '\n') return 0;
        if (lexer->current[0] == delimiter
            && (!multiline || (lexer->current[1] == delimiter && lexer->current[2] == delimiter))) {
            if (multiline) {
                lexer->current += 3;
                lexer->column += 3;
            } else {
                ++lexer->current;
                ++lexer->column;
            }
            return append_token(lexer, kind, start, line, column);
        }
        if (delimiter == '"' && *lexer->current == '\\' && lexer->current[1]) {
            const char *escape = lexer->current + 1;
            if (!valid_escape(lexer->current + 1)) return 0;
            lexer->current += 2;
            lexer->column += 2;
            if (escape[0] == 'u') {
                lexer->current += 4;
                lexer->column += 4;
            } else if (escape[0] == 'U') {
                lexer->current += 8;
                lexer->column += 8;
            }
            continue;
        }
        if (delimiter == '"' && *lexer->current == '\\') return 0;
        if (*lexer->current == '\n') {
            ++lexer->line;
            lexer->column = 1;
        } else {
            ++lexer->column;
        }
        ++lexer->current;
    }
    return 0;
}

static int is_number_character(char character) {
    return isalnum((unsigned char)character) || character == '_' || character == '.'
        || character == ':' || character == '+' || character == '-';
}

static int looks_like_date(const char *start, const char *end) {
    size_t length = (size_t)(end - start);
    return length == 10 && start[4] == '-' && start[7] == '-'
        && isdigit((unsigned char)start[0]) && isdigit((unsigned char)start[1])
        && isdigit((unsigned char)start[2]) && isdigit((unsigned char)start[3])
        && isdigit((unsigned char)start[5]) && isdigit((unsigned char)start[6])
        && isdigit((unsigned char)start[8]) && isdigit((unsigned char)start[9]);
}

static int scan_number_or_datetime(Lexer *lexer) {
    const char *start = lexer->current;
    size_t line = lexer->line;
    size_t column = lexer->column;
    RadarTokenKind kind = RADAR_TOKEN_INTEGER;
    while (is_number_character(*lexer->current)) {
        if (*lexer->current == '.' || *lexer->current == 'e' || *lexer->current == 'E') {
            kind = RADAR_TOKEN_FLOAT;
        }
        if (*lexer->current == ':' || *lexer->current == 'T' || *lexer->current == 't'
            || *lexer->current == 'Z' || *lexer->current == 'z') {
            kind = RADAR_TOKEN_DATETIME;
        }
        ++lexer->current;
        ++lexer->column;
    }
    if (looks_like_date(start, lexer->current)) kind = RADAR_TOKEN_DATETIME;
    {
        const char *cursor;
        for (cursor = start; cursor < lexer->current; ++cursor) {
            if (*cursor == '_' && (cursor == start || cursor + 1 == lexer->current
                                   || cursor[-1] == '_' || cursor[1] == '_')) return 0;
        }
    }
    return append_token(lexer, kind, start, line, column);
}

static void skip_space_and_comments(Lexer *lexer) {
    for (;;) {
        while (*lexer->current == ' ' || *lexer->current == '\t' || *lexer->current == '\r'
               || *lexer->current == '\n') {
            if (*lexer->current == '\n') {
                ++lexer->line;
                lexer->column = 1;
            } else {
                ++lexer->column;
            }
            ++lexer->current;
        }
        if (*lexer->current != '#') return;
        while (*lexer->current && *lexer->current != '\n') {
            ++lexer->current;
            ++lexer->column;
        }
    }
}

int radar_lex(const char *source, RadarTokenList *tokens, RadarLexerError *error) {
    Lexer lexer = {source, 1, 1, tokens};
    if (!source || !tokens) {
        set_error(error, "invalid lexer input", 1, 1);
        return 0;
    }
    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
    if (error) *error = (RadarLexerError){0};
    while (1) {
        const char *start;
        size_t line;
        size_t column;
        RadarTokenKind kind;
        skip_space_and_comments(&lexer);
        start = lexer.current;
        line = lexer.line;
        column = lexer.column;
        if (!*start) {
            if (!append_token(&lexer, RADAR_TOKEN_EOF, start, line, column)) {
                set_error(error, "out of memory", line, column);
                radar_tokens_free(tokens);
                return 0;
            }
            return 1;
        }
        if (is_identifier_start(*start)) {
            do { ++lexer.current; ++lexer.column; } while (is_identifier_part(*lexer.current));
            if (!append_token(&lexer, literal_kind(start, (size_t)(lexer.current - start)), start,
                              line, column)) {
                set_error(error, "out of memory", line, column);
                radar_tokens_free(tokens);
                return 0;
            }
            continue;
        }
        if (isdigit((unsigned char)*start)) {
            if (!scan_number_or_datetime(&lexer)) {
                set_error(error, "invalid number or date literal", line, column);
                radar_tokens_free(tokens);
                return 0;
            }
            continue;
        }
        if (*start == '"' || *start == '\'') {
            kind = *start == '"' ? RADAR_TOKEN_STRING : RADAR_TOKEN_LITERAL_STRING;
            if (!scan_quoted_string(&lexer, *start, kind)) {
                set_error(error, "unterminated or invalid string literal", line, column);
                radar_tokens_free(tokens);
                return 0;
            }
            continue;
        }
        kind = scan_punctuation(&lexer);
        if (kind == RADAR_TOKEN_ERROR) {
            set_error(error, "unexpected character", line, column);
            radar_tokens_free(tokens);
            return 0;
        }
        if (!append_token(&lexer, kind, start, line, column)) {
            set_error(error, "out of memory", line, column);
            radar_tokens_free(tokens);
            return 0;
        }
    }
}

void radar_tokens_free(RadarTokenList *tokens) {
    free(tokens->items);
    *tokens = (RadarTokenList){0};
}

const char *radar_token_name(RadarTokenKind kind) {
    static const char *names[] = {
        "eof", "error", "identifier", "string", "literal-string", "multiline-string", "multiline-literal-string", "datetime", "integer", "float", "true", "false",
        "[", "]", "[[", "]]", "{", "}", ",", ".", "=", "+", "-"
    };
    return kind < (RadarTokenKind)(sizeof(names) / sizeof(names[0])) ? names[kind] : "unknown";
}