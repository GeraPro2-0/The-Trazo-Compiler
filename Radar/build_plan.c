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

#include <stdlib.h>
#include <string.h>

typedef struct {
    const RadarTokenList *tokens;
    size_t current;
    RadarBuildPlan *plan;
    RadarPlanError *error;
    int in_build_table;
    size_t block_depth;
} PlanReader;

static char *copy_token(const RadarToken *token) {
    size_t offset = 0;
    size_t length = token->length;
    char *value;
    if (length >= 2 && ((token->start[0] == '"' && token->start[length - 1] == '"')
                        || (token->start[0] == '\'' && token->start[length - 1] == '\''))) {
        offset = 1;
        length -= 2;
    }
    value = malloc(length + 1);
    if (!value) return NULL;
    memcpy(value, token->start + offset, length);
    value[length] = '\0';
    return value;
}

static int fail(PlanReader *reader, const char *message, const RadarToken *token) {
    reader->error->message = message;
    reader->error->line = token->line;
    reader->error->column = token->column;
    return 0;
}

static int key_is(const RadarToken *token, const char *english, const char *spanish) {
    size_t length = strlen(english);
    if (token->length == length && strncmp(token->start, english, length) == 0) return 1;
    length = strlen(spanish);
    return token->length == length && strncmp(token->start, spanish, length) == 0;
}

static int append_value(char **items, size_t *count, const RadarToken *token) {
    if (*count == RADAR_BUILD_PLAN_MAX_ITEMS) return 0;
    items[*count] = copy_token(token);
    if (!items[*count]) return 0;
    ++*count;
    return 1;
}

static int read_array(PlanReader *reader, char **items, size_t *count) {
    const RadarToken *token;
    ++reader->current;
    while (reader->tokens->items[reader->current].kind != RADAR_TOKEN_RIGHT_BRACKET) {
        token = &reader->tokens->items[reader->current];
        if (token->kind == RADAR_TOKEN_EOF) return fail(reader, "unterminated plan array", token);
        if (token->kind == RADAR_TOKEN_STRING || token->kind == RADAR_TOKEN_LITERAL_STRING) {
            if (!append_value(items, count, token)) return fail(reader, "too many plan values", token);
        }
        ++reader->current;
    }
    ++reader->current;
    return 1;
}

static int assign_value(PlanReader *reader, const RadarToken *key, const RadarToken *value) {
    char **target = NULL;
    if (key_is(key, "compiler", "compilador")) target = &reader->plan->compiler;
    else if (key_is(key, "kind", "tipo")) target = &reader->plan->kind;
    else if (key_is(key, "output", "salida")) target = &reader->plan->output;
    else if (key_is(key, "std-c", "estandar-c")) target = &reader->plan->standard;
    if (!target) return 1;
    free(*target);
    *target = copy_token(value);
    return *target != NULL;
}

int radar_build_plan(const RadarTokenList *tokens, RadarBuildPlan *plan,
                     RadarPlanError *error) {
    PlanReader reader = {tokens, 0, plan, error, 0, 0};
    const RadarToken *token;
    memset(plan, 0, sizeof(*plan));
    *error = (RadarPlanError){0};
    while (tokens->items[reader.current].kind != RADAR_TOKEN_EOF) {
        token = &tokens->items[reader.current];
        if (token->kind == RADAR_TOKEN_LEFT_BRACE) { ++reader.block_depth; ++reader.current; continue; }
        if (token->kind == RADAR_TOKEN_RIGHT_BRACE) { if (reader.block_depth) --reader.block_depth; ++reader.current; continue; }
        if (token->kind == RADAR_TOKEN_LEFT_BRACKET) {
            ++reader.current;
            reader.in_build_table = reader.current < tokens->count
                && key_is(&tokens->items[reader.current], "build", "construccion");
            while (tokens->items[reader.current].kind != RADAR_TOKEN_RIGHT_BRACKET
                   && tokens->items[reader.current].kind != RADAR_TOKEN_EOF) ++reader.current;
            if (tokens->items[reader.current].kind == RADAR_TOKEN_RIGHT_BRACKET) ++reader.current;
            continue;
        }
        if (!reader.in_build_table || reader.block_depth != 0
            || token->kind != RADAR_TOKEN_IDENTIFIER) { ++reader.current; continue; }
        if (reader.current + 1 >= tokens->count || tokens->items[reader.current + 1].kind != RADAR_TOKEN_EQUAL) {
            ++reader.current;
            continue;
        }
        if (token->length >= sizeof("include") - 1 && key_is(token, "include", "incluir")) {
            reader.current += 2;
            if (tokens->items[reader.current].kind == RADAR_TOKEN_LEFT_BRACKET
                && !read_array(&reader, plan->includes, &plan->include_count)) return 0;
            continue;
        }
        if (key_is(token, "flags", "banderas") || key_is(token, "sources", "fuentes")) {
            char **items = key_is(token, "flags", "banderas") ? plan->flags : plan->sources;
            size_t *count = key_is(token, "flags", "banderas") ? &plan->flag_count : &plan->source_count;
            reader.current += 2;
            if (tokens->items[reader.current].kind == RADAR_TOKEN_LEFT_BRACKET
                && !read_array(&reader, items, count)) return 0;
            continue;
        }
        reader.current += 2;
        if (reader.current >= tokens->count || !assign_value(&reader, token, &tokens->items[reader.current])) {
            return fail(&reader, "unable to create build plan", token);
        }
        ++reader.current;
    }
    return 1;
}

void radar_build_plan_free(RadarBuildPlan *plan) {
    size_t index;
    free(plan->compiler); free(plan->kind); free(plan->output); free(plan->standard);
    for (index = 0; index < plan->include_count; ++index) free(plan->includes[index]);
    for (index = 0; index < plan->source_count; ++index) free(plan->sources[index]);
    for (index = 0; index < plan->flag_count; ++index) free(plan->flags[index]);
    *plan = (RadarBuildPlan){0};
}