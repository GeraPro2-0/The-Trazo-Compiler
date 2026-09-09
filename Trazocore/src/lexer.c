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

#define TRAZO_TOKEN_INITIAL_CAPACITY 64

typedef struct {
    const char *text_en;
    const char *text_es;
    TrazoTokenKind kind;
} TrazoKeyword;

static const TrazoKeyword keywords[] = {
    {"break", "romper", TRAZO_TOKEN_KW_BREAK},
    {"case", "caso", TRAZO_TOKEN_KW_CASE},
    {"char", "caracter", TRAZO_TOKEN_KW_CHAR},
    {"const", "constante", TRAZO_TOKEN_KW_CONST},
    {"continue", "continuar", TRAZO_TOKEN_KW_CONTINUE},
    {"default", "pordefecto", TRAZO_TOKEN_KW_DEFAULT},
    {"else", "sino", TRAZO_TOKEN_KW_ELSE},
    {"enum", "enumeracion", TRAZO_TOKEN_KW_ENUM},
    {"extern", "externo", TRAZO_TOKEN_KW_EXTERN},
    {"float", "flotante", TRAZO_TOKEN_KW_FLOAT},
    {"for", "para", TRAZO_TOKEN_KW_FOR},
    {"if", "si", TRAZO_TOKEN_KW_IF},
    {"int", "entero", TRAZO_TOKEN_KW_INT},
    {"return", "retornar", TRAZO_TOKEN_KW_RETURN},
    {"static", "estatico", TRAZO_TOKEN_KW_STATIC},
    {"struct", "estructura", TRAZO_TOKEN_KW_STRUCT},
    {"switch", "cambiar", TRAZO_TOKEN_KW_SWITCH},
    {"typedef", "definirtipo", TRAZO_TOKEN_KW_TYPEDEF},
    {"union", "union", TRAZO_TOKEN_KW_UNION},
    {"void", "vacio", TRAZO_TOKEN_KW_VOID},
    {"volatile", "volatil", TRAZO_TOKEN_KW_VOLATILE},
    {"while", "mientras", TRAZO_TOKEN_KW_WHILE},
    {"importC", "importarC", TRAZO_TOKEN_KW_IMPORT_C},
    {"import", "importar", TRAZO_TOKEN_KW_IMPORT},
    {"from", "desde", TRAZO_TOKEN_KW_FROM},
    {"as", "como", TRAZO_TOKEN_KW_AS},
    {"func", "funcion", TRAZO_TOKEN_KW_FUNC},
    {"unsafe", "inseguro", TRAZO_TOKEN_KW_UNSAFE},
    {"export", "exportar", TRAZO_TOKEN_KW_EXPORT}
};

static int is_alpha(char character) {
    return (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z')
        || character == '_';
}

static int is_digit(char character) {
    return character >= '0' && character <= '9';
}

static int is_alphanumeric(char character) {
    return is_alpha(character) || is_digit(character);
}

static void set_error(TrazoLexerError *error, const char *message, size_t line, size_t column) {
    if (error) {
        error->message = message;
        error->line = line;
        error->column = column;
    }
}

static int append_token(TrazoTokenList *tokens, TrazoTokenKind kind, const char *start,
                        size_t length, size_t line, size_t column) {
    TrazoToken *items;
    size_t capacity;

    if (tokens->count == tokens->capacity) {
        capacity = tokens->capacity == 0
            ? TRAZO_TOKEN_INITIAL_CAPACITY
            : tokens->capacity * 2;
        items = realloc(tokens->items, capacity * sizeof(*items));
        if (!items) {
            return 0;
        }
        tokens->items = items;
        tokens->capacity = capacity;
    }

    tokens->items[tokens->count++] = (TrazoToken){kind, start, length, line, column};
    return 1;
}

static TrazoTokenKind keyword_kind(const char *start, size_t length) {
    size_t index;

    for (index = 0; index < sizeof(keywords) / sizeof(keywords[0]); ++index) {
        if ((strlen(keywords[index].text_en) == length
                && memcmp(keywords[index].text_en, start, length) == 0)
            || (strlen(keywords[index].text_es) == length
                && memcmp(keywords[index].text_es, start, length) == 0)) {
            return keywords[index].kind;
        }
    }

    return TRAZO_TOKEN_IDENTIFIER;
}

static int scan_escape(const char **cursor, size_t *column) {
    if (**cursor == '\0' || **cursor == '\n') {
        return 0;
    }
    ++*cursor;
    ++*column;
    return 1;
}

static TrazoTokenKind punctuation_kind(char character) {
    switch (character) {
        case '(': return TRAZO_TOKEN_LEFT_PAREN;
        case ')': return TRAZO_TOKEN_RIGHT_PAREN;
        case '{': return TRAZO_TOKEN_LEFT_BRACE;
        case '}': return TRAZO_TOKEN_RIGHT_BRACE;
        case '[': return TRAZO_TOKEN_LEFT_BRACKET;
        case ']': return TRAZO_TOKEN_RIGHT_BRACKET;
        case ',': return TRAZO_TOKEN_COMMA;
        case ';': return TRAZO_TOKEN_SEMICOLON;
        case ':': return TRAZO_TOKEN_COLON;
        case '.': return TRAZO_TOKEN_DOT;
        case '+': return TRAZO_TOKEN_PLUS;
        case '-': return TRAZO_TOKEN_MINUS;
        case '*': return TRAZO_TOKEN_STAR;
        case '/': return TRAZO_TOKEN_SLASH;
        case '%': return TRAZO_TOKEN_PERCENT;
        case '&': return TRAZO_TOKEN_AMPERSAND;
        case '|': return TRAZO_TOKEN_PIPE;
        case '^': return TRAZO_TOKEN_CARET;
        case '~': return TRAZO_TOKEN_TILDE;
        case '!': return TRAZO_TOKEN_BANG;
        case '=': return TRAZO_TOKEN_EQUAL;
        case '<': return TRAZO_TOKEN_LESS;
        case '>': return TRAZO_TOKEN_GREATER;
        default: return TRAZO_TOKEN_ERROR;
    }
}

int trazo_lex(const char *source, TrazoTokenList *tokens, TrazoLexerError *error) {
    const char *cursor;
    size_t line = 1;
    size_t column = 1;

    if (!source || !tokens) {
        set_error(error, "invalid lexer input", line, column);
        return 0;
    }

    tokens->items = NULL;
    tokens->count = 0;
    tokens->capacity = 0;
    if (error) {
        error->message = NULL;
        error->line = 0;
        error->column = 0;
    }

    cursor = source;
    while (*cursor) {
        const char *start;
        size_t start_column;
        TrazoTokenKind kind;

        if (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') {
            ++cursor;
            ++column;
            continue;
        }
        if (*cursor == '\n') {
            ++cursor;
            ++line;
            column = 1;
            continue;
        }
        if (*cursor == '/' && cursor[1] == '/') {
            cursor += 2;
            column += 2;
            while (*cursor && *cursor != '\n') {
                ++cursor;
                ++column;
            }
            continue;
        }
        if (*cursor == '/' && cursor[1] == '*') {
            cursor += 2;
            column += 2;
            while (*cursor && !(*cursor == '*' && cursor[1] == '/')) {
                if (*cursor == '\n') {
                    ++line;
                    column = 1;
                    ++cursor;
                } else {
                    ++cursor;
                    ++column;
                }
            }
            if (!*cursor) {
                set_error(error, "unterminated block comment", line, column);
                trazo_tokens_free(tokens);
                return 0;
            }
            cursor += 2;
            column += 2;
            continue;
        }

        start = cursor;
        start_column = column;
        if (is_alpha(*cursor)) {
            do {
                ++cursor;
                ++column;
            } while (is_alphanumeric(*cursor));
            kind = keyword_kind(start, (size_t)(cursor - start));
            if (!append_token(tokens, kind, start, (size_t)(cursor - start), line, start_column)) {
                set_error(error, "out of memory", line, start_column);
                trazo_tokens_free(tokens);
                return 0;
            }
            continue;
        }
        if (is_digit(*cursor)) {
            kind = TRAZO_TOKEN_INTEGER;
            while (is_digit(*cursor)) {
                ++cursor;
                ++column;
            }
            if (*cursor == '.' && is_digit(cursor[1])) {
                kind = TRAZO_TOKEN_FLOAT;
                ++cursor;
                ++column;
                while (is_digit(*cursor)) {
                    ++cursor;
                    ++column;
                }
            }
            if ((kind == TRAZO_TOKEN_FLOAT || kind == TRAZO_TOKEN_INTEGER)
                && (*cursor == 'f' || *cursor == 'F')) {
                kind = TRAZO_TOKEN_FLOAT;
                ++cursor;
                ++column;
            }
            if (!append_token(tokens, kind, start, (size_t)(cursor - start), line, start_column)) {
                set_error(error, "out of memory", line, start_column);
                trazo_tokens_free(tokens);
                return 0;
            }
            continue;
        }
        if (*cursor == '"' || *cursor == '\'') {
            char delimiter = *cursor++;
            kind = delimiter == '"' ? TRAZO_TOKEN_STRING : TRAZO_TOKEN_CHARACTER;
            ++column;
            while (*cursor && *cursor != delimiter && *cursor != '\n') {
                if (*cursor == '\\') {
                    ++cursor;
                    ++column;
                    if (!scan_escape(&cursor, &column)) {
                        break;
                    }
                } else {
                    ++cursor;
                    ++column;
                }
            }
            if (*cursor != delimiter) {
                set_error(error, "unterminated literal", line, start_column);
                trazo_tokens_free(tokens);
                return 0;
            }
            ++cursor;
            ++column;
            if (!append_token(tokens, kind, start, (size_t)(cursor - start), line, start_column)) {
                set_error(error, "out of memory", line, start_column);
                trazo_tokens_free(tokens);
                return 0;
            }
            continue;
        }

        if (cursor[0] == '+' && cursor[1] == '+') {
            kind = TRAZO_TOKEN_PLUS_PLUS;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '-' && cursor[1] == '-') {
            kind = TRAZO_TOKEN_MINUS_MINUS;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '-' && cursor[1] == '>') {
            kind = TRAZO_TOKEN_ARROW;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '=' && cursor[1] == '=') {
            kind = TRAZO_TOKEN_EQUAL_EQUAL;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '!' && cursor[1] == '=') {
            kind = TRAZO_TOKEN_BANG_EQUAL;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '<' && cursor[1] == '=') {
            kind = TRAZO_TOKEN_LESS_EQUAL;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '>' && cursor[1] == '=') {
            kind = TRAZO_TOKEN_GREATER_EQUAL;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '&' && cursor[1] == '&') {
            kind = TRAZO_TOKEN_AND_AND;
            cursor += 2;
            column += 2;
        } else if (cursor[0] == '|' && cursor[1] == '|') {
            kind = TRAZO_TOKEN_OR_OR;
            cursor += 2;
            column += 2;
        } else {
            kind = punctuation_kind(*cursor);
            if (kind == TRAZO_TOKEN_ERROR) {
                set_error(error, "unexpected character", line, column);
                trazo_tokens_free(tokens);
                return 0;
            }
            ++cursor;
            ++column;
        }
        if (!append_token(tokens, kind, start, (size_t)(cursor - start), line, start_column)) {
            set_error(error, "out of memory", line, start_column);
            trazo_tokens_free(tokens);
            return 0;
        }
    }

    if (!append_token(tokens, TRAZO_TOKEN_EOF, cursor, 0, line, column)) {
        set_error(error, "out of memory", line, column);
        trazo_tokens_free(tokens);
        return 0;
    }
    return 1;
}

void trazo_tokens_free(TrazoTokenList *tokens) {
    if (tokens) {
        free(tokens->items);
        tokens->items = NULL;
        tokens->count = 0;
        tokens->capacity = 0;
    }
}

const char *trazo_token_name(TrazoTokenKind kind) {
    static const char *names[] = {
        "eof", "error", "identifier", "integer", "float", "string", "character",
        "break", "case", "char", "const", "continue", "default", "else",
        "enum", "extern", "float", "for", "if", "int", "return", "static",
        "struct", "switch", "typedef", "union", "void", "volatile",
        "while", "func", "unsafe", "export", "import", "from", "as", "import_c", "left_paren", "right_paren",
        "left_brace", "right_brace", "left_bracket", "right_bracket", "comma", "semicolon",
        "colon", "dot", "arrow", "plus", "plus_plus", "minus", "minus_minus", "star", "slash", "percent", "ampersand",
        "pipe", "caret", "tilde", "bang", "equal", "equal_equal", "bang_equal", "less",
        "less_equal", "greater", "greater_equal", "and_and", "or_or"
    };
    size_t count = sizeof(names) / sizeof(names[0]);
    return kind >= 0 && (size_t)kind < count ? names[kind] : "unknown";
}
