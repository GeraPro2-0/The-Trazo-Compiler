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

#ifndef RADAR_H
#define RADAR_H

#include <stddef.h>

typedef enum {
    RADAR_TOKEN_EOF,
    RADAR_TOKEN_ERROR,
    RADAR_TOKEN_IDENTIFIER,
    RADAR_TOKEN_STRING,
    RADAR_TOKEN_LITERAL_STRING,
    RADAR_TOKEN_MULTILINE_STRING,
    RADAR_TOKEN_MULTILINE_LITERAL_STRING,
    RADAR_TOKEN_DATETIME,
    RADAR_TOKEN_INTEGER,
    RADAR_TOKEN_FLOAT,
    RADAR_TOKEN_TRUE,
    RADAR_TOKEN_FALSE,
    RADAR_TOKEN_LEFT_BRACKET,
    RADAR_TOKEN_RIGHT_BRACKET,
    RADAR_TOKEN_DOUBLE_LEFT_BRACKET,
    RADAR_TOKEN_DOUBLE_RIGHT_BRACKET,
    RADAR_TOKEN_LEFT_BRACE,
    RADAR_TOKEN_RIGHT_BRACE,
    RADAR_TOKEN_COMMA,
    RADAR_TOKEN_DOT,
    RADAR_TOKEN_EQUAL,
    RADAR_TOKEN_PLUS,
    RADAR_TOKEN_MINUS,
} RadarTokenKind;

typedef struct {
    RadarTokenKind kind;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
} RadarToken;

typedef struct {
    RadarToken *items;
    size_t count;
    size_t capacity;
} RadarTokenList;

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} RadarLexerError;

int radar_lex(const char *source, RadarTokenList *tokens, RadarLexerError *error);
void radar_tokens_free(RadarTokenList *tokens);
const char *radar_token_name(RadarTokenKind kind);

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} RadarParserError;

int radar_parse(const RadarTokenList *tokens, RadarParserError *error);

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} RadarValidationError;

int radar_validate_tokens(const RadarTokenList *tokens, RadarValidationError *error);

#define RADAR_BUILD_PLAN_MAX_ITEMS 128

typedef struct {
    char *compiler;
    char *kind;
    char *output;
    char *standard;
    char *includes[RADAR_BUILD_PLAN_MAX_ITEMS];
    size_t include_count;
    char *sources[RADAR_BUILD_PLAN_MAX_ITEMS];
    size_t source_count;
    char *flags[RADAR_BUILD_PLAN_MAX_ITEMS];
    size_t flag_count;
} RadarBuildPlan;

typedef struct {
    const char *message;
    size_t line;
    size_t column;
} RadarPlanError;

int radar_build_plan(const RadarTokenList *tokens, RadarBuildPlan *plan,
                     RadarPlanError *error);
void radar_build_plan_free(RadarBuildPlan *plan);

#endif