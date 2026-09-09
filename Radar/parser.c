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

#include <string.h>

typedef struct {
    const RadarTokenList *tokens;
    size_t current;
    RadarParserError *error;
} Parser;

static int check(Parser *parser, RadarTokenKind kind) {
    return parser->tokens->items[parser->current].kind == kind;
}

static int fail(Parser *parser, const char *message) {
    const RadarToken *token = &parser->tokens->items[parser->current];
    parser->error->message = message;
    parser->error->line = token->line;
    parser->error->column = token->column;
    return 0;
}

static int match(Parser *parser, RadarTokenKind kind) {
    if (!check(parser, kind)) return 0;
    ++parser->current;
    return 1;
}

static int expect(Parser *parser, RadarTokenKind kind, const char *message) {
    return match(parser, kind) || fail(parser, message);
}

static int expression(Parser *parser);

static int key_part(Parser *parser) {
    return match(parser, RADAR_TOKEN_IDENTIFIER)
        || match(parser, RADAR_TOKEN_STRING)
        || match(parser, RADAR_TOKEN_INTEGER)
        || fail(parser, "expected key");
}

static int key_path(Parser *parser) {
    if (!key_part(parser)) return 0;
    while (match(parser, RADAR_TOKEN_DOT)) {
        if (!key_part(parser)) return 0;
    }
    return 1;
}

static int primary(Parser *parser) {
    if (match(parser, RADAR_TOKEN_STRING) || match(parser, RADAR_TOKEN_LITERAL_STRING)
        || match(parser, RADAR_TOKEN_MULTILINE_STRING)
        || match(parser, RADAR_TOKEN_MULTILINE_LITERAL_STRING)
        || match(parser, RADAR_TOKEN_DATETIME)
        || match(parser, RADAR_TOKEN_INTEGER) || match(parser, RADAR_TOKEN_FLOAT)
        || match(parser, RADAR_TOKEN_TRUE) || match(parser, RADAR_TOKEN_FALSE)) return 1;
    if (match(parser, RADAR_TOKEN_LEFT_BRACKET)) {
        if (!check(parser, RADAR_TOKEN_RIGHT_BRACKET)) {
            if (!expression(parser)) return 0;
            while (match(parser, RADAR_TOKEN_COMMA)) {
                if (check(parser, RADAR_TOKEN_RIGHT_BRACKET)) break;
                if (!expression(parser)) return 0;
            }
        }
        return expect(parser, RADAR_TOKEN_RIGHT_BRACKET, "expected ']' after array");
    }
    if (match(parser, RADAR_TOKEN_LEFT_BRACE)) {
        if (!check(parser, RADAR_TOKEN_RIGHT_BRACE)) {
            if (!key_path(parser) || !expect(parser, RADAR_TOKEN_EQUAL, "expected '=' in inline table")
                || !expression(parser)) return 0;
            while (match(parser, RADAR_TOKEN_COMMA)) {
                if (check(parser, RADAR_TOKEN_RIGHT_BRACE)) break;
                if (!key_path(parser) || !expect(parser, RADAR_TOKEN_EQUAL, "expected '=' in inline table")
                    || !expression(parser)) return 0;
            }
        }
        return expect(parser, RADAR_TOKEN_RIGHT_BRACE, "expected '}' after inline table");
    }
    return fail(parser, "expected expression");
}

static int expression(Parser *parser) {
    if (match(parser, RADAR_TOKEN_MINUS) || match(parser, RADAR_TOKEN_PLUS)) {
        if (!match(parser, RADAR_TOKEN_INTEGER) && !match(parser, RADAR_TOKEN_FLOAT)) {
            return fail(parser, "expected numeric value after sign");
        }
        return 1;
    }
    return primary(parser);
}

static int statement(Parser *parser) {
    if (!key_path(parser)) return 0;
    if (!expect(parser, RADAR_TOKEN_EQUAL, "expected '=' in TOML assignment")) return 0;
    return expression(parser);
}

int radar_parse(const RadarTokenList *tokens, RadarParserError *error) {
    Parser parser = {tokens, 0, error};
    *error = (RadarParserError){0};
    while (!check(&parser, RADAR_TOKEN_EOF)) {
        if (match(&parser, RADAR_TOKEN_DOUBLE_LEFT_BRACKET)) {
            if (!key_path(&parser)
                || !expect(&parser, RADAR_TOKEN_DOUBLE_RIGHT_BRACKET,
                           "expected ']]' after array table name")) return 0;
        } else if (match(&parser, RADAR_TOKEN_LEFT_BRACKET)) {
            if (!key_path(&parser)
                || !expect(&parser, RADAR_TOKEN_RIGHT_BRACKET, "expected ']' after table name")) return 0;
        } else if (!statement(&parser)) {
            return 0;
        }
    }
    return 1;
}