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
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int token_equals(const RadarToken *token, const char *text) {
    size_t length = strlen(text);
    return token->length == length && strncmp(token->start, text, length) == 0;
}

static int valid_date_fields(const char *text, size_t length) {
    int year;
    int month;
    int day;
    if (length != 10 || text[4] != '-' || text[7] != '-') return 0;
    if (!isdigit((unsigned char)text[0]) || !isdigit((unsigned char)text[9])) return 0;
    year = (text[0] - '0') * 1000 + (text[1] - '0') * 100
        + (text[2] - '0') * 10 + text[3] - '0';
    month = (text[5] - '0') * 10 + text[6] - '0';
    day = (text[8] - '0') * 10 + text[9] - '0';
    if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) return 0;
    if (month == 2 && day > 29) return 0;
    if (month == 2 && day == 29 && (year % 4 != 0 || (year % 100 == 0 && year % 400 != 0))) return 0;
    return 1;
}

static int valid_time_fields(const char *text, size_t length) {
    size_t index;
    int hour;
    int minute;
    int second;
    if (length < 8 || text[2] != ':' || text[5] != ':') return 0;
    for (index = 0; index < 8; ++index) {
        if (index != 2 && index != 5 && !isdigit((unsigned char)text[index])) return 0;
    }
    hour = (text[0] - '0') * 10 + text[1] - '0';
    minute = (text[3] - '0') * 10 + text[4] - '0';
    second = (text[6] - '0') * 10 + text[7] - '0';
    return hour <= 23 && minute <= 59 && second <= 59;
}

static int valid_datetime(const RadarToken *token) {
    size_t index = 19;
    if (token->length == 10) return valid_date_fields(token->start, token->length);
    if (token->length >= 8 && token->start[2] == ':' && token->start[5] == ':') {
        return valid_time_fields(token->start, token->length);
    }
    if (token->length < 19 || !valid_date_fields(token->start, 10)
        || (token->start[10] != 'T' && token->start[10] != 't' && token->start[10] != ' ')
        || !valid_time_fields(token->start + 11, token->length - 11)) return 0;
    while (index < token->length && isdigit((unsigned char)token->start[index])) ++index;
    if (index > 19 && token->start[19] != '.') return 0;
    if (index < token->length && token->start[index] == '.') {
        ++index;
        if (index == token->length || !isdigit((unsigned char)token->start[index])) return 0;
        while (index < token->length && isdigit((unsigned char)token->start[index])) ++index;
    }
    if (index == token->length) return 1;
    if (token->start[index] == 'Z' || token->start[index] == 'z') return index + 1 == token->length;
    if ((token->start[index] == '+' || token->start[index] == '-')
        && index + 6 == token->length && token->start[index + 3] == ':') {
        return isdigit((unsigned char)token->start[index + 1])
            && isdigit((unsigned char)token->start[index + 2])
            && isdigit((unsigned char)token->start[index + 4])
            && isdigit((unsigned char)token->start[index + 5])
            && (token->start[index + 1] - '0') * 10 + token->start[index + 2] - '0' <= 23
            && (token->start[index + 4] - '0') * 10 + token->start[index + 5] - '0' <= 59;
    }
    return 0;
}

static int valid_integer(const RadarToken *token) {
    char buffer[128];
    char *end;
    char *normalized;
    size_t index;
    size_t normalized_length = 0;
    long long value;
    int base = 10;
    if (token->length >= sizeof(buffer)) return 0;
    normalized = buffer;
    for (index = 0; index < token->length; ++index) {
        if (token->start[index] != '_') normalized[normalized_length++] = token->start[index];
    }
    normalized[normalized_length] = '\0';
    if (normalized[0] == '0' && (normalized[1] == 'x' || normalized[1] == 'X')) {
        base = 16;
    } else if (normalized[0] == '0' && (normalized[1] == 'o' || normalized[1] == 'O')) {
        base = 8;
        memmove(normalized + 1, normalized + 2, normalized_length - 1);
        --normalized_length;
        normalized[normalized_length] = '\0';
    } else if (normalized[0] == '0' && (normalized[1] == 'b' || normalized[1] == 'B')) {
        base = 2;
        memmove(normalized + 1, normalized + 2, normalized_length - 1);
        --normalized_length;
        normalized[normalized_length] = '\0';
    }
    errno = 0;
    value = strtoll(normalized, &end, base);
    (void)value;
    return errno != ERANGE && *end == '\0';
}

static int valid_float(const RadarToken *token) {
    char buffer[128];
    char *end;
    size_t index;
    size_t normalized_length = 0;
    double value;
    if (token->length >= sizeof(buffer)) return 0;
    for (index = 0; index < token->length; ++index) {
        if (token->start[index] != '_') buffer[normalized_length++] = token->start[index];
    }
    buffer[normalized_length] = '\0';
    errno = 0;
    value = strtod(buffer, &end);
    (void)value;
    return errno != ERANGE && *end == '\0';
}

int radar_validate_tokens(const RadarTokenList *tokens, RadarValidationError *error) {
    size_t index;
    *error = (RadarValidationError){0};
    for (index = 0; index < tokens->count; ++index) {
        const RadarToken *token = &tokens->items[index];
        int valid = 1;
        const char *message = NULL;
        if (token->kind == RADAR_TOKEN_INTEGER) {
            valid = valid_integer(token);
            message = "invalid integer literal";
        } else if (token->kind == RADAR_TOKEN_FLOAT) {
            valid = valid_float(token);
            message = "invalid floating-point literal";
        } else if (token->kind == RADAR_TOKEN_DATETIME) {
            valid = valid_datetime(token);
            message = "invalid date or time literal";
        } else if (token->kind == RADAR_TOKEN_STRING || token->kind == RADAR_TOKEN_MULTILINE_STRING) {
            valid = !token_equals(token, "\"\"");
            message = "invalid string literal";
        }
        if (!valid) {
            error->message = message;
            error->line = token->line;
            error->column = token->column;
            return 0;
        }
    }
    return 1;
}