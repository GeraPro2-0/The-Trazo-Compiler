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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef DECNUMDIGITS
#define DECNUMDIGITS 34
#endif

#include "../decNumber/decNumber-icu-368/decContext.h"
#include "../decNumber/decNumber-icu-368/decNumber.h"
#include "../decNumber/decNumber-icu-368/decimal32.h"
#include "../decNumber/decNumber-icu-368/decimal64.h"
#include "../decNumber/decNumber-icu-368/decimal128.h"

/* Bootstrap-friendly runtime/memory lifecycle wrappers. */
int trazo_memory_init(void) {
    /* Placeholder: integrate SMM here later. Return success for bootstrap. */
    return 1;
}

void trazo_memory_shutdown(void) {
    /* Placeholder: shutdown SMM when available. */
}

int trazo_runtime_init(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
    if (!setlocale(LC_ALL, "")) {
        setlocale(LC_CTYPE, "C.UTF-8");
    }
#endif
    return 1;
}

void trazo_runtime_destroy(void) {
    /* Placeholder: global runtime teardown. */
}

/* Simple allocation wrappers to centralize future SMM integration. */
void* trazo_alloc(size_t size) {
    return malloc(size);
}

void* trazo_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

void trazo_free(void* ptr) {
    free(ptr);
}

void* trazo_calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* p = trazo_alloc(total);
    if (p) memset(p, 0, total);
    return p;
}

typedef enum {
    RV_VOID = 0,
    RV_NULL,
    RV_BOOL,
    RV_INT,
    RV_FLOAT,
    RV_DECIMAL,
    RV_DEC32,
    RV_DEC64,
    RV_DEC128,
    RV_STRING,
    RV_PTR,
    RV_STRUCT,
    RV_ARRAY
} RuntimeValueKind;

typedef struct RuntimeArray RuntimeArray;
typedef struct RuntimeStruct RuntimeStruct;
typedef struct RuntimeStructType RuntimeStructType;

typedef struct {
    RuntimeValueKind kind;
    union {
        int64_t int_value;
        double float_value;
        decNumber* decimal_value;
        decimal32 dec32_value;
        decimal64 dec64_value;
        decimal128 dec128_value;
        char* string_value;
        void* ptr_value;
        RuntimeArray* array_value;
    } as;
} RuntimeValue;

struct RuntimeArray {
    RuntimeValue* items;
    size_t count;
};

struct RuntimeStruct {
    char* type_name;
    char** names;
    RuntimeValue* values;
    size_t count;
    size_t capacity;
};

struct RuntimeStructType {
    char* name;
    char** field_names;
    TrazoType* field_types;
    size_t count;
    size_t capacity;
};

typedef struct {
    char* name;
    RuntimeValue value;
    int scope_depth;
} RuntimeVar;

typedef struct {
    TrazoStmt* stmt;
    void* symbol;
} RuntimeExtern;

typedef struct {
    RuntimeVar* vars;
    size_t var_count;
    size_t var_capacity;
    TrazoStmt** funcs;
    size_t func_count;
    size_t func_capacity;
    RuntimeExtern* externs;
    size_t extern_count;
    size_t extern_capacity;
    RuntimeStructType* struct_types;
    size_t struct_type_count;
    size_t struct_type_capacity;
    int scope_depth;
    int unsafe_depth;
    RuntimeValue return_value;
} Runtime;

typedef enum {
    EXEC_OK = 0,
    EXEC_BREAK,
    EXEC_CONTINUE,
    EXEC_RETURN,
    EXEC_ERROR
} ExecResult;

static char* token_text(const TrazoToken* token) {
    char* text = (char*)trazo_alloc(token->length + 1);
    if (!text) return NULL;
    memcpy(text, token->start, token->length);
    text[token->length] = '\0';
    return text;
}

static char* strdup_len(const char* s, size_t len) {
    char* out = (char*)trazo_alloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static int token_equals(const TrazoToken* token, const char* text) {
    size_t len = strlen(text);
    return token->length == len && memcmp(token->start, text, len) == 0;
}

static int token_decimal_digits(const char* text) {
    int digits = 0;
    int in_exp = 0;
    for (const char* p = text; *p; p++) {
        if (*p == 'e' || *p == 'E') {
            in_exp = 1;
            continue;
        }
        if (!in_exp && *p >= '0' && *p <= '9') digits++;
    }
    return digits > 0 ? digits : 1;
}

static decContext decimal_context(int digits) {
    decContext ctx;
    decContextDefault(&ctx, DEC_INIT_BASE);
    ctx.digits = digits > 0 ? digits : 34;
    ctx.emax = DEC_MAX_EMAX;
    ctx.emin = DEC_MIN_EMIN;
    ctx.traps = 0;
    return ctx;
}

static decNumber* decimal_alloc_digits(int digits) {
    size_t units = ((size_t)digits + DECDPUN - 1) / DECDPUN;
    size_t bytes = sizeof(decNumber);
    if (units > DECNUMUNITS) bytes += (units - DECNUMUNITS) * sizeof(decNumberUnit);
    return (decNumber*)trazo_calloc(1, bytes);
}

static RuntimeValue value_decimal_from_string(const char* text);
static char* decimal_value_to_string(RuntimeValue value);

static RuntimeValue value_void(void) {
    RuntimeValue v;
    memset(&v, 0, sizeof(v));
    v.kind = RV_VOID;
    return v;
}

static RuntimeValue value_null(void) {
    RuntimeValue v = value_void();
    v.kind = RV_NULL;
    return v;
}

static RuntimeValue value_bool(int b) {
    RuntimeValue v = value_void();
    v.kind = RV_BOOL;
    v.as.int_value = b ? 1 : 0;
    return v;
}

static RuntimeValue value_int(int64_t n) {
    RuntimeValue v = value_void();
    v.kind = RV_INT;
    v.as.int_value = n;
    return v;
}

static RuntimeValue value_float(double n) {
    RuntimeValue v = value_void();
    v.kind = RV_FLOAT;
    v.as.float_value = n;
    return v;
}

static RuntimeValue value_decimal_from_string(const char* text) {
    RuntimeValue v = value_void();
    int digits = token_decimal_digits(text ? text : "0");
    decContext ctx = decimal_context(digits);
    v.kind = RV_DECIMAL;
    v.as.decimal_value = decimal_alloc_digits(digits);
    if (!v.as.decimal_value) return value_null();
    decNumberFromString(v.as.decimal_value, text ? text : "0", &ctx);
    return v;
}

static RuntimeValue value_dec32_from_string(const char* text) {
    RuntimeValue v = value_void();
    decContext ctx;
    decContextDefault(&ctx, DEC_INIT_DECIMAL32);
    ctx.traps = 0;
    v.kind = RV_DEC32;
    decimal32FromString(&v.as.dec32_value, text ? text : "0", &ctx);
    return v;
}

static RuntimeValue value_dec64_from_string(const char* text) {
    RuntimeValue v = value_void();
    decContext ctx;
    decContextDefault(&ctx, DEC_INIT_DECIMAL64);
    ctx.traps = 0;
    v.kind = RV_DEC64;
    decimal64FromString(&v.as.dec64_value, text ? text : "0", &ctx);
    return v;
}

static RuntimeValue value_dec128_from_string(const char* text) {
    RuntimeValue v = value_void();
    decContext ctx;
    decContextDefault(&ctx, DEC_INIT_DECIMAL128);
    ctx.traps = 0;
    v.kind = RV_DEC128;
    decimal128FromString(&v.as.dec128_value, text ? text : "0", &ctx);
    return v;
}

static RuntimeValue value_ptr(void* ptr) {
    RuntimeValue v = value_void();
    v.kind = ptr ? RV_PTR : RV_NULL;
    v.as.ptr_value = ptr;
    return v;
}

static RuntimeValue value_struct_new(RuntimeStruct* s) {
    RuntimeValue v = value_void();
    v.kind = s ? RV_STRUCT : RV_NULL;
    v.as.ptr_value = s;
    return v;
}

static RuntimeValue value_array(RuntimeArray* array) {
    RuntimeValue v = value_void();
    v.kind = array ? RV_ARRAY : RV_NULL;
    v.as.array_value = array;
    return v;
}

static char* unescape_string(const char* src, size_t len) {
    char* out = (char*)trazo_alloc(len + 1);
    if (!out) return NULL;
    size_t ri = 0;
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        if (c == '\\' && i + 1 < len) {
            char esc = src[++i];
            switch (esc) {
                case 'n': out[ri++] = '\n'; break;
                case 'r': out[ri++] = '\r'; break;
                case 't': out[ri++] = '\t'; break;
                case '\\': out[ri++] = '\\'; break;
                case '"': out[ri++] = '"'; break;
                case '\'': out[ri++] = '\''; break;
                case '0': out[ri++] = '\0'; break;
                default:
                    out[ri++] = esc;
                    break;
            }
        } else {
            out[ri++] = c;
        }
    }
    out[ri] = '\0';
    return out;
}

static RuntimeValue value_string_from_token(const TrazoToken* token) {
    RuntimeValue v = value_void();
    size_t start = 0;
    size_t len = token->length;

    v.kind = RV_STRING;
    if (len >= 2 && token->start[0] == '"' && token->start[len - 1] == '"') {
        start = 1;
        len -= 2;
    }

    char* un = unescape_string(token->start + start, len);
    if (!un) {
        v.kind = RV_NULL;
        return v;
    }
    v.as.string_value = un;
    return v;
}

static RuntimeValue value_string_owned(char* text) {
    RuntimeValue v = value_void();
    v.kind = text ? RV_STRING : RV_NULL;
    v.as.string_value = text;
    return v;
}

static RuntimeValue copy_value(RuntimeValue value) {
    RuntimeValue copy = value;
    if (value.kind == RV_STRING && value.as.string_value) {
        size_t len = strlen(value.as.string_value);
        copy.as.string_value = (char*)trazo_alloc(len + 1);
        if (copy.as.string_value) memcpy(copy.as.string_value, value.as.string_value, len + 1);
    } else if (value.kind == RV_ARRAY && value.as.array_value) {
        RuntimeArray* array = (RuntimeArray*)trazo_calloc(1, sizeof(RuntimeArray));
        if (array) {
            array->count = value.as.array_value->count;
            array->items = (RuntimeValue*)trazo_calloc(array->count, sizeof(RuntimeValue));
            if (array->items) {
                for (size_t i = 0; i < array->count; i++) {
                    array->items[i] = copy_value(value.as.array_value->items[i]);
                }
                copy.as.array_value = array;
            } else {
                trazo_free(array);
                copy.kind = RV_NULL;
            }
        }
    }
    else if (value.kind == RV_STRUCT && value.as.ptr_value) {
        RuntimeStruct* s = (RuntimeStruct*)value.as.ptr_value;
        RuntimeStruct* ns = (RuntimeStruct*)trazo_calloc(1, sizeof(RuntimeStruct));
        if (ns) {
            ns->type_name = s->type_name ? strdup_len(s->type_name, strlen(s->type_name)) : NULL;
            ns->count = s->count;
            ns->capacity = s->count;
            if (ns->count) {
                ns->names = (char**)trazo_calloc(ns->count, sizeof(char*));
                ns->values = (RuntimeValue*)trazo_calloc(ns->count, sizeof(RuntimeValue));
                if (ns->names && ns->values) {
                    for (size_t i = 0; i < ns->count; i++) {
                        ns->names[i] = strdup_len(s->names[i], strlen(s->names[i]));
                        ns->values[i] = copy_value(s->values[i]);
                    }
                    copy.as.ptr_value = ns;
                } else {
                    if (ns->names) { trazo_free(ns->names); ns->names = NULL; }
                    if (ns->values) { trazo_free(ns->values); ns->values = NULL; }
                    trazo_free(ns);
                    copy.kind = RV_NULL;
                }
            } else {
                copy.as.ptr_value = ns;
            }
        }
    }
    else if (value.kind == RV_DECIMAL && value.as.decimal_value) {
        int digits = value.as.decimal_value->digits;
        copy.as.decimal_value = decimal_alloc_digits(digits);
        if (copy.as.decimal_value) {
            size_t units = ((size_t)digits + DECDPUN - 1) / DECDPUN;
            size_t bytes = sizeof(decNumber);
            if (units > DECNUMUNITS) bytes += (units - DECNUMUNITS) * sizeof(decNumberUnit);
            memcpy(copy.as.decimal_value, value.as.decimal_value, bytes);
        } else {
            copy.kind = RV_NULL;
        }
    }
    return copy;
}

static void free_value(RuntimeValue value) {
    if (value.kind == RV_STRING) trazo_free(value.as.string_value);
    if (value.kind == RV_DECIMAL) trazo_free(value.as.decimal_value);
    if (value.kind == RV_ARRAY && value.as.array_value) {
        for (size_t i = 0; i < value.as.array_value->count; i++) {
            free_value(value.as.array_value->items[i]);
        }
        trazo_free(value.as.array_value->items);
        trazo_free(value.as.array_value);
    }
    if (value.kind == RV_STRUCT && value.as.ptr_value) {
        RuntimeStruct* s = (RuntimeStruct*)value.as.ptr_value;
        for (size_t i = 0; i < s->count; i++) {
            trazo_free(s->names[i]);
            free_value(s->values[i]);
        }
        trazo_free(s->type_name);
        trazo_free(s->names);
        trazo_free(s->values);
        trazo_free(s);
    }
}

static int is_truthy(RuntimeValue value) {
    switch (value.kind) {
        case RV_BOOL:
        case RV_INT: return value.as.int_value != 0;
        case RV_FLOAT: return value.as.float_value != 0.0;
        case RV_DECIMAL: return value.as.decimal_value && !decNumberIsZero(value.as.decimal_value);
        case RV_DEC32:
        case RV_DEC64:
        case RV_DEC128: {
            char* text = decimal_value_to_string(value);
            int truthy = text && strcmp(text, "0") != 0 && strcmp(text, "0E+0") != 0;
            trazo_free(text);
            return truthy;
        }
        case RV_STRING: return value.as.string_value && value.as.string_value[0] != '\0';
        case RV_PTR: return value.as.ptr_value != NULL;
        default: return 0;
    }
}

static double as_float(RuntimeValue value) {
    if (value.kind == RV_FLOAT) return value.as.float_value;
    if (value.kind == RV_INT || value.kind == RV_BOOL) return (double)value.as.int_value;
    return 0.0;
}

static int64_t as_int(RuntimeValue value) {
    if (value.kind == RV_INT || value.kind == RV_BOOL) return value.as.int_value;
    if (value.kind == RV_FLOAT) return (int64_t)value.as.float_value;
    if (value.kind == RV_PTR) return (int64_t)(intptr_t)value.as.ptr_value;
    return 0;
}

static char* decimal_value_to_string(RuntimeValue value) {
    char stack[256];
    char* text = NULL;
    size_t len;

    switch (value.kind) {
        case RV_DECIMAL:
            if (!value.as.decimal_value) return strdup_len("0", 1);
            len = (size_t)value.as.decimal_value->digits + 32;
            text = (char*)trazo_alloc(len);
            if (!text) return NULL;
            decNumberToString(value.as.decimal_value, text);
            return text;
        case RV_DEC32:
            decimal32ToString(&value.as.dec32_value, stack);
            break;
        case RV_DEC64:
            decimal64ToString(&value.as.dec64_value, stack);
            break;
        case RV_DEC128:
            decimal128ToString(&value.as.dec128_value, stack);
            break;
        case RV_FLOAT:
            snprintf(stack, sizeof(stack), "%.17g", value.as.float_value);
            break;
        case RV_INT:
        case RV_BOOL:
            snprintf(stack, sizeof(stack), "%lld", (long long)value.as.int_value);
            break;
        default:
            snprintf(stack, sizeof(stack), "0");
            break;
    }

    return strdup_len(stack, strlen(stack));
}

static int is_decimal_value(RuntimeValue value) {
    return value.kind == RV_DECIMAL || value.kind == RV_DEC32 || value.kind == RV_DEC64 || value.kind == RV_DEC128;
}

static RuntimeValue coerce_to_declared_type(RuntimeValue value, TrazoTypeKind type) {
    char* text;
    RuntimeValue coerced;

    if (type != TRAZO_TYPE_DECIMAL && type != TRAZO_TYPE_DEC32 &&
        type != TRAZO_TYPE_DEC64 && type != TRAZO_TYPE_DEC128) {
        return copy_value(value);
    }

    text = decimal_value_to_string(value);
    switch (type) {
        case TRAZO_TYPE_DECIMAL:
            coerced = value_decimal_from_string(text ? text : "0");
            break;
        case TRAZO_TYPE_DEC32:
            coerced = value_dec32_from_string(text ? text : "0");
            break;
        case TRAZO_TYPE_DEC64:
            coerced = value_dec64_from_string(text ? text : "0");
            break;
        case TRAZO_TYPE_DEC128:
            coerced = value_dec128_from_string(text ? text : "0");
            break;
        default:
            coerced = copy_value(value);
            break;
    }
    trazo_free(text);
    return coerced;
}

static int64_t sizeof_type(const TrazoType* type) {
    if (!type) return 0;
    if (type->pointer_depth > 0) return (int64_t)sizeof(void*);
    switch (type->kind) {
        case TRAZO_TYPE_BOOL: return (int64_t)sizeof(int);
        case TRAZO_TYPE_INT:
        case TRAZO_TYPE_I64: return (int64_t)sizeof(int64_t);
        case TRAZO_TYPE_I8: return (int64_t)sizeof(int8_t);
        case TRAZO_TYPE_I16: return (int64_t)sizeof(int16_t);
        case TRAZO_TYPE_I32: return (int64_t)sizeof(int32_t);
        case TRAZO_TYPE_U8: return (int64_t)sizeof(uint8_t);
        case TRAZO_TYPE_U16: return (int64_t)sizeof(uint16_t);
        case TRAZO_TYPE_U32: return (int64_t)sizeof(uint32_t);
        case TRAZO_TYPE_U64: return (int64_t)sizeof(uint64_t);
        case TRAZO_TYPE_BYTE: return (int64_t)sizeof(uint8_t);
        case TRAZO_TYPE_CHAR: return (int64_t)sizeof(char);
        case TRAZO_TYPE_RUNE: return (int64_t)sizeof(int32_t);
        case TRAZO_TYPE_FLOAT:
        case TRAZO_TYPE_F32: return (int64_t)sizeof(float);
        case TRAZO_TYPE_DOUBLE:
        case TRAZO_TYPE_F64: return (int64_t)sizeof(double);
        case TRAZO_TYPE_DECIMAL:
        case TRAZO_TYPE_DEC32:
        case TRAZO_TYPE_DEC64:
        case TRAZO_TYPE_DEC128: return (int64_t)sizeof(void*);
        case TRAZO_TYPE_STRING: return (int64_t)sizeof(void*);
        case TRAZO_TYPE_PTR: return (int64_t)sizeof(void*);
        case TRAZO_TYPE_STRUCT: return (int64_t)sizeof(void*);
        case TRAZO_TYPE_VOID: return 0;
        default: return (int64_t)sizeof(RuntimeValue);
    }
}

static char* runtime_value_to_string(RuntimeValue value) {
    char buffer[64];

    switch (value.kind) {
        case RV_STRING:
            return strdup_len(value.as.string_value ? value.as.string_value : "", value.as.string_value ? strlen(value.as.string_value) : 0);
        case RV_BOOL:
            return strdup_len(value.as.int_value ? "true" : "false", value.as.int_value ? 4 : 5);
        case RV_INT:
            snprintf(buffer, sizeof(buffer), "%lld", (long long)as_int(value));
            break;
        case RV_FLOAT:
            snprintf(buffer, sizeof(buffer), "%.17g", as_float(value));
            break;
        case RV_DECIMAL:
        case RV_DEC32:
        case RV_DEC64:
        case RV_DEC128:
            return decimal_value_to_string(value);
        case RV_PTR:
            snprintf(buffer, sizeof(buffer), "%p", value.as.ptr_value);
            break;
        case RV_NULL:
            return strdup_len("null", 4);
        case RV_ARRAY:
            return strdup_len("[array]", 7);
        case RV_STRUCT:
            return strdup_len("{struct}", 8);
        default:
            return strdup_len("", 0);
    }
    return strdup_len(buffer, strlen(buffer));
}

static int64_t runtime_value_as_int(RuntimeValue value) {
    char* text;
    int64_t result = 0;

    if (value.kind == RV_INT || value.kind == RV_BOOL) return value.as.int_value;
    if (value.kind == RV_FLOAT) return (int64_t)value.as.float_value;
    if (is_decimal_value(value)) {
        text = decimal_value_to_string(value);
        if (text) {
            result = strtoll(text, NULL, 10);
            trazo_free(text);
        }
        return result;
    }
    if (value.kind == RV_STRING && value.as.string_value) {
        return strtoll(value.as.string_value, NULL, 10);
    }
    if (value.kind == RV_PTR) return (int64_t)(intptr_t)value.as.ptr_value;
    return 0;
}

static double runtime_value_as_float(RuntimeValue value) {
    char* text;
    double result = 0.0;

    if (value.kind == RV_FLOAT) return value.as.float_value;
    if (value.kind == RV_INT || value.kind == RV_BOOL) return (double)value.as.int_value;
    if (is_decimal_value(value)) {
        text = decimal_value_to_string(value);
        if (text) {
            result = strtod(text, NULL);
            trazo_free(text);
        }
        return result;
    }
    if (value.kind == RV_STRING && value.as.string_value) {
        return strtod(value.as.string_value, NULL);
    }
    return 0.0;
}

static RuntimeValue cast_runtime_value(RuntimeValue value, TrazoType type) {
    RuntimeValue result = value_void();
    switch (type.kind) {
        case TRAZO_TYPE_BOOL:
            result = value_bool(is_truthy(value));
            break;
        case TRAZO_TYPE_INT:
        case TRAZO_TYPE_I8:
        case TRAZO_TYPE_I16:
        case TRAZO_TYPE_I32:
        case TRAZO_TYPE_I64:
        case TRAZO_TYPE_U8:
        case TRAZO_TYPE_U16:
        case TRAZO_TYPE_U32:
        case TRAZO_TYPE_U64:
        case TRAZO_TYPE_BYTE:
        case TRAZO_TYPE_CHAR:
        case TRAZO_TYPE_RUNE:
            result = value_int(runtime_value_as_int(value));
            break;
        case TRAZO_TYPE_FLOAT:
        case TRAZO_TYPE_F32:
        case TRAZO_TYPE_F64:
            result = value_float(runtime_value_as_float(value));
            break;
        case TRAZO_TYPE_DECIMAL: {
            char* text = runtime_value_to_string(value);
            result = value_decimal_from_string(text ? text : "0");
            trazo_free(text);
            break;
        }
        case TRAZO_TYPE_DEC32: {
            char* text = runtime_value_to_string(value);
            result = value_dec32_from_string(text ? text : "0");
            trazo_free(text);
            break;
        }
        case TRAZO_TYPE_DEC64: {
            char* text = runtime_value_to_string(value);
            result = value_dec64_from_string(text ? text : "0");
            trazo_free(text);
            break;
        }
        case TRAZO_TYPE_DEC128: {
            char* text = runtime_value_to_string(value);
            result = value_dec128_from_string(text ? text : "0");
            trazo_free(text);
            break;
        }
        case TRAZO_TYPE_STRING: {
            char* text = runtime_value_to_string(value);
            result = value_string_owned(text);
            break;
        }
        case TRAZO_TYPE_PTR:
            result = value.kind == RV_PTR ? value_ptr(value.as.ptr_value) : value_null();
            break;
        default:
            result = copy_value(value);
            break;
    }
    free_value(value);
    return result;
}

static RuntimeVar* find_var(Runtime* rt, const TrazoToken* name) {
    for (size_t i = rt->var_count; i > 0; i--) {
        RuntimeVar* var = &rt->vars[i - 1];
        if (name->length == strlen(var->name) && memcmp(name->start, var->name, name->length) == 0) {
            return var;
        }
    }
    return NULL;
}

static int define_var(Runtime* rt, const TrazoToken* name, RuntimeValue value) {
    RuntimeVar* var;
    char* copied_name;

    if (rt->var_count == rt->var_capacity) {
        size_t next_capacity = rt->var_capacity < 16 ? 16 : rt->var_capacity * 2;
        RuntimeVar* next_vars = (RuntimeVar*)trazo_realloc(rt->vars, next_capacity * sizeof(RuntimeVar));
        if (!next_vars) return 0;
        rt->vars = next_vars;
        rt->var_capacity = next_capacity;
    }

    copied_name = token_text(name);
    if (!copied_name) return 0;

    var = &rt->vars[rt->var_count++];
    var->name = copied_name;
    var->value = copy_value(value);
    var->scope_depth = rt->scope_depth;
    return 1;
}

static int register_func(Runtime* rt, TrazoStmt* func) {
    if (rt->func_count == rt->func_capacity) {
        size_t next_capacity = rt->func_capacity < 8 ? 8 : rt->func_capacity * 2;
        TrazoStmt** next_funcs = (TrazoStmt**)trazo_realloc(rt->funcs, next_capacity * sizeof(TrazoStmt*));
        if (!next_funcs) return 0;
        rt->funcs = next_funcs;
        rt->func_capacity = next_capacity;
    }
    rt->funcs[rt->func_count++] = func;
    return 1;
}

static int register_extern(Runtime* rt, TrazoStmt* extern_stmt) {
    if (rt->extern_count == rt->extern_capacity) {
        size_t next_capacity = rt->extern_capacity < 8 ? 8 : rt->extern_capacity * 2;
        RuntimeExtern* next_externs = (RuntimeExtern*)trazo_realloc(rt->externs, next_capacity * sizeof(RuntimeExtern));
        if (!next_externs) return 0;
        rt->externs = next_externs;
        rt->extern_capacity = next_capacity;
    }
    rt->externs[rt->extern_count].stmt = extern_stmt;
    rt->externs[rt->extern_count].symbol = NULL;
    rt->extern_count++;
    return 1;
}

static TrazoStmt* find_func(Runtime* rt, const TrazoToken* name) {
    for (size_t i = rt->func_count; i > 0; i--) {
        TrazoStmt* fn = rt->funcs[i - 1];
        if (name->length == fn->token.length && memcmp(name->start, fn->token.start, name->length) == 0) {
            return fn;
        }
    }
    return NULL;
}

static TrazoStmt* find_extern(Runtime* rt, const TrazoToken* name) {
    for (size_t i = rt->extern_count; i > 0; i--) {
        RuntimeExtern* ext = &rt->externs[i - 1];
        if (name->length == ext->stmt->token.length && memcmp(name->start, ext->stmt->token.start, name->length) == 0) {
            return ext->stmt;
        }
    }
    return NULL;
}

static RuntimeStructType* find_struct_type(Runtime* rt, const char* name, size_t length) {
    for (size_t i = rt->struct_type_count; i > 0; i--) {
        RuntimeStructType* type = &rt->struct_types[i - 1];
        if (length == strlen(type->name) && memcmp(name, type->name, length) == 0) {
            return type;
        }
    }
    return NULL;
}

static void print_runtime_error(const TrazoToken* token, const char* message);

static int register_struct_type(Runtime* rt, TrazoStmt* stmt) {
    if (!stmt || stmt->kind != TRAZO_STMT_STRUCT) return 0;
    size_t name_len = stmt->token.length;
    if (find_struct_type(rt, stmt->token.start, name_len)) {
        print_runtime_error(&stmt->token, "duplicate struct declaration");
        return 0;
    }

    if (rt->struct_type_count == rt->struct_type_capacity) {
        size_t next_capacity = rt->struct_type_capacity < 8 ? 8 : rt->struct_type_capacity * 2;
        RuntimeStructType* next_types = (RuntimeStructType*)trazo_realloc(rt->struct_types, next_capacity * sizeof(RuntimeStructType));
        if (!next_types) return 0;
        rt->struct_types = next_types;
        rt->struct_type_capacity = next_capacity;
    }

    size_t index = rt->struct_type_count;
    RuntimeStructType* type = &rt->struct_types[index];
    memset(type, 0, sizeof(RuntimeStructType));
    type->name = strdup_len(stmt->token.start, name_len);
    type->count = stmt->child_count;
    if (type->count > 0) {
        type->capacity = type->count;
        type->field_names = (char**)trazo_calloc(type->count, sizeof(char*));
        type->field_types = (TrazoType*)trazo_calloc(type->count, sizeof(TrazoType));
        if (!type->field_names || !type->field_types) {
            trazo_free(type->name);
            trazo_free(type->field_names);
            trazo_free(type->field_types);
            return 0;
        }
        for (size_t i = 0; i < type->count; i++) {
            TrazoStmt* field = stmt->children[i];
            type->field_names[i] = strdup_len(field->token.start, field->token.length);
            type->field_types[i] = field->type;
            if (!type->field_names[i]) {
                for (size_t j = 0; j < i; j++) trazo_free(type->field_names[j]);
                trazo_free(type->field_names);
                trazo_free(type->field_types);
                trazo_free(type->name);
                return 0;
            }
        }
    }
    rt->struct_type_count = index + 1;
    return 1;
}

static int struct_type_has_field(RuntimeStructType* type, const char* name, size_t length) {
    if (!type) return 0;
    for (size_t i = 0; i < type->count; i++) {
        if (strlen(type->field_names[i]) == length && memcmp(type->field_names[i], name, length) == 0) {
            return 1;
        }
    }
    return 0;
}

static int set_struct_type_name(Runtime* rt, RuntimeStruct* s, const char* type_name, size_t type_name_len) {
    if (!s || !type_name) return 0;
    if (s->type_name) {
        trazo_free(s->type_name);
        s->type_name = NULL;
    }
    s->type_name = strdup_len(type_name, type_name_len);
    RuntimeStructType* type = find_struct_type(rt, type_name, type_name_len);
    if (!type) return 1;

    if (s->count == 0 && type->count > 0) {
        size_t original_count = s->count;
        s->count = type->count;
        s->capacity = type->count;
        s->names = (char**)trazo_calloc(s->count, sizeof(char*));
        s->values = (RuntimeValue*)trazo_calloc(s->count, sizeof(RuntimeValue));
        if (!s->names || !s->values) {
            trazo_free(s->names);
            trazo_free(s->values);
            s->names = NULL;
            s->values = NULL;
            s->count = original_count;
            s->capacity = original_count;
            return 0;
        }
        for (size_t i = 0; i < s->count; i++) {
            s->names[i] = strdup_len(type->field_names[i], strlen(type->field_names[i]));
            s->values[i] = value_null();
            if (!s->names[i]) {
                for (size_t j = 0; j < i; j++) trazo_free(s->names[j]);
                trazo_free(s->names);
                trazo_free(s->values);
                s->names = NULL;
                s->values = NULL;
                s->count = original_count;
                s->capacity = original_count;
                return 0;
            }
        }
    }
    return 1;
}

static void free_struct_types(Runtime* rt) {
    for (size_t i = 0; i < rt->struct_type_count; i++) {
        RuntimeStructType* type = &rt->struct_types[i];
        trazo_free(type->name);
        for (size_t j = 0; j < type->count; j++) {
            trazo_free(type->field_names[j]);
        }
        trazo_free(type->field_names);
        trazo_free(type->field_types);
    }
    trazo_free(rt->struct_types);
}

static void print_runtime_error(const TrazoToken* token, const char* message);
static RuntimeValue eval_expr(Runtime* rt, TrazoExpr* expr, int* ok);

static RuntimeValue eval_extern_call(Runtime* rt, TrazoStmt* extern_decl, TrazoExpr* expr, int* ok) {
    if (expr->arg_count != extern_decl->param_count) {
        print_runtime_error(&expr->token, "wrong argument count for extern call");
        *ok = 0;
        return value_void();
    }

    if (expr->arg_count > 16) {
        print_runtime_error(&expr->token, "extern call argument count exceeds supported limit");
        *ok = 0;
        return value_void();
    }

    RuntimeValue args[16];
    for (size_t i = 0; i < expr->arg_count; i++) {
        args[i] = eval_expr(rt, expr->args[i], ok);
        if (!*ok) {
            for (size_t j = 0; j < i; j++) free_value(args[j]);
            return value_void();
        }
    }

    const TrazoToken* name = &extern_decl->token;
    RuntimeValue result = value_void();

    if (token_equals(name, "puts")) {
        if (expr->arg_count != 1 || args[0].kind != RV_STRING) {
            print_runtime_error(name, "puts requires one string argument");
            *ok = 0;
        } else {
            result = value_int(puts(args[0].as.string_value));
        }
    } else if (token_equals(name, "putchar")) {
        if (expr->arg_count != 1) {
            print_runtime_error(name, "putchar requires one argument");
            *ok = 0;
        } else {
            result = value_int(putchar((int)as_int(args[0])));
        }
    } else if (token_equals(name, "strlen")) {
        if (expr->arg_count != 1 || args[0].kind != RV_STRING) {
            print_runtime_error(name, "strlen requires one string argument");
            *ok = 0;
        } else {
            result = value_int((int64_t)strlen(args[0].as.string_value));
        }
    } else if (token_equals(name, "strcmp")) {
        if (expr->arg_count != 2 || args[0].kind != RV_STRING || args[1].kind != RV_STRING) {
            print_runtime_error(name, "strcmp requires two string arguments");
            *ok = 0;
        } else {
            result = value_int((int64_t)strcmp(args[0].as.string_value, args[1].as.string_value));
        }
    } else if (token_equals(name, "getenv")) {
        if (expr->arg_count != 1 || args[0].kind != RV_STRING) {
            print_runtime_error(name, "getenv requires one string argument");
            *ok = 0;
        } else {
            const char* value = getenv(args[0].as.string_value);
            result = value_string_owned(value ? strdup(value) : NULL);
        }
    } else if (token_equals(name, "atoi")) {
        if (expr->arg_count != 1 || args[0].kind != RV_STRING) {
            print_runtime_error(name, "atoi requires one string argument");
            *ok = 0;
        } else {
            result = value_int((int64_t)atoi(args[0].as.string_value));
        }
    } else if (token_equals(name, "exit")) {
        if (expr->arg_count != 1) {
            print_runtime_error(name, "exit requires one integer argument");
            *ok = 0;
        } else {
            int code = (int)as_int(args[0]);
            for (size_t i = 0; i < expr->arg_count; i++) free_value(args[i]);
            exit(code);
        }
    } else {
        print_runtime_error(name, "unknown extern function");
        *ok = 0;
    }

    for (size_t i = 0; i < expr->arg_count; i++) {
        free_value(args[i]);
    }
    return result;
}

static TrazoStmt* find_func_name(Runtime* rt, const char* name) {
    TrazoToken token;
    memset(&token, 0, sizeof(token));
    token.start = name;
    token.length = strlen(name);
    return find_func(rt, &token);
}

static void assign_var(RuntimeVar* var, RuntimeValue value) {
    free_value(var->value);
    var->value = copy_value(value);
}

static void enter_scope(Runtime* rt) {
    rt->scope_depth++;
}

static void leave_scope(Runtime* rt) {
    while (rt->var_count > 0 && rt->vars[rt->var_count - 1].scope_depth == rt->scope_depth) {
        RuntimeVar* var = &rt->vars[--rt->var_count];
        trazo_free(var->name);
        free_value(var->value);
    }
    if (rt->scope_depth > 0) rt->scope_depth--;
}

static void print_runtime_error(const TrazoToken* token, const char* message) {
    fprintf(stderr, "Runtime error at line %d, column %d: %s\n", token->line, token->column, message);
}

static int require_unsafe(Runtime* rt, const TrazoToken* token, const char* operation) {
    if (rt->unsafe_depth > 0) return 1;
    fprintf(stderr, "Runtime error at line %d, column %d: %s requires unsafe.MAX\n",
            token->line,
            token->column,
            operation);
    return 0;
}

static void print_value(RuntimeValue value) {
    char* decimal_text;
    switch (value.kind) {
        case RV_VOID: printf("void"); break;
        case RV_NULL: printf("null"); break;
        case RV_BOOL: printf(value.as.int_value ? "true" : "false"); break;
        case RV_INT: printf("%lld", (long long)as_int(value)); break;
        case RV_FLOAT: printf("%g", as_float(value)); break;
        case RV_DECIMAL:
        case RV_DEC32:
        case RV_DEC64:
        case RV_DEC128:
            decimal_text = decimal_value_to_string(value);
            printf("%s", decimal_text ? decimal_text : "0");
            trazo_free(decimal_text);
            break;
        case RV_STRING: printf("%s", value.as.string_value ? value.as.string_value : ""); break;
        case RV_PTR: printf("%p", value.as.ptr_value); break;
        case RV_ARRAY:
            if (!value.as.array_value) { printf("[]"); break; }
            printf("[");
            for (size_t i = 0; i < value.as.array_value->count; i++) {
                if (i) printf(", ");
                print_value(value.as.array_value->items[i]);
            }
            printf("]");
            break;
        case RV_STRUCT:
            if (!value.as.ptr_value) { printf("{}"); break; }
            {
                RuntimeStruct* s = (RuntimeStruct*)value.as.ptr_value;
                printf("{");
                for (size_t i = 0; i < s->count; i++) {
                    if (i) printf(", ");
                    printf("%s: ", s->names[i] ? s->names[i] : "");
                    print_value(s->values[i]);
                }
                printf("}");
            }
            break;
        default:
            printf("<unknown>");
            break;
    }
}

static char* read_entire_file(const char* path) {
    FILE* file = fopen(path, "rb");
    long size;
    char* data;
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

static int write_entire_file(const char* path, const char* data, const char* mode) {
    FILE* file = fopen(path, mode);
    size_t len;
    size_t written;

    if (!file) return 0;
    len = data ? strlen(data) : 0;
    written = fwrite(data ? data : "", 1, len, file);
    fclose(file);
    return written == len;
}

static int file_exists(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return 0;
    fclose(file);
    return 1;
}

static char* read_line_stdin(void) {
    size_t capacity = 128;
    size_t count = 0;
    char* data = (char*)trazo_alloc(capacity);
    int ch;

    if (!data) return NULL;

    while ((ch = getchar()) != EOF && ch != '\n') {
        if (count + 1 >= capacity) {
            size_t next_capacity = capacity * 2;
            char* next_data = (char*)trazo_realloc(data, next_capacity);
            if (!next_data) {
                trazo_free(data);
                return NULL;
            }
            data = next_data;
            capacity = next_capacity;
        }
        data[count++] = (char)ch;
    }

    data[count] = '\0';
    return data;
}

static RuntimeValue eval_expr(Runtime* rt, TrazoExpr* expr, int* ok);
static ExecResult exec_stmt(Runtime* rt, TrazoStmt* stmt);
static RuntimeValue* eval_address(Runtime* rt, TrazoExpr* expr, int* ok);

static RuntimeValue* eval_address(Runtime* rt, TrazoExpr* expr, int* ok) {
    if (!expr) return NULL;

    if (expr->kind == TRAZO_EXPR_IDENTIFIER) {
        RuntimeVar* var = find_var(rt, &expr->token);
        if (!var) {
            print_runtime_error(&expr->token, "undefined variable");
            *ok = 0;
            return NULL;
        }
        return &var->value;
    }

    if (expr->kind == TRAZO_EXPR_UNARY && expr->token.type == TRAZO_TOKEN_STAR) {
        RuntimeValue ptr = eval_expr(rt, expr->right, ok);
        RuntimeValue* address;
        if (!*ok) return NULL;
        if (ptr.kind != RV_PTR || !ptr.as.ptr_value) {
            print_runtime_error(&expr->token, "cannot dereference non-pointer");
            *ok = 0;
            return NULL;
        }
        address = (RuntimeValue*)ptr.as.ptr_value;
        return address;
    }

    if (expr->kind == TRAZO_EXPR_INDEX) {
        RuntimeValue* base = eval_address(rt, expr->left, ok);
        RuntimeValue idx;
        size_t index;
        if (!*ok || !base) return NULL;
        if (base->kind != RV_ARRAY || !base->as.array_value) {
            print_runtime_error(&expr->token, "indexed address target is not an array");
            *ok = 0;
            return NULL;
        }
        idx = eval_expr(rt, expr->right, ok);
        if (!*ok) return NULL;
        index = (size_t)as_int(idx);
        free_value(idx);
        if (index >= base->as.array_value->count) {
            print_runtime_error(&expr->token, "index out of bounds");
            *ok = 0;
            return NULL;
        }
        return &base->as.array_value->items[index];
    }

    print_runtime_error(&expr->token, "expression is not addressable");
    *ok = 0;
    return NULL;
}

static RuntimeValue eval_call(Runtime* rt, TrazoExpr* expr, int* ok) {
    TrazoExpr* callee = expr->left;
    RuntimeValue result = value_void();
    TrazoStmt* user_func;

    if (!callee || callee->kind != TRAZO_EXPR_IDENTIFIER) {
        print_runtime_error(&expr->token, "unsupported call target");
        *ok = 0;
        return result;
    }

    if (token_equals(&callee->token, "malloc") || token_equals(&callee->token, "reservar") ||
        token_equals(&callee->token, "alloc") || token_equals(&callee->token, "asignar_mem")) {
        RuntimeValue size;
        if (!require_unsafe(rt, &callee->token, "manual allocation")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "allocation requires one size argument");
            *ok = 0;
            return result;
        }
        size = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        result = value_ptr(trazo_alloc((size_t)as_int(size)));
        free_value(size);
        return result;
    }

    if (token_equals(&callee->token, "calloc") || token_equals(&callee->token, "asignar_cero")) {
        RuntimeValue count;
        RuntimeValue size;
        if (!require_unsafe(rt, &callee->token, "calloc")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 2) {
            print_runtime_error(&callee->token, "calloc requires count and size arguments");
            *ok = 0;
            return result;
        }
        count = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        size = eval_expr(rt, expr->args[1], ok);
        if (!*ok) {
            free_value(count);
            return result;
        }
        result = value_ptr(trazo_calloc((size_t)as_int(count), (size_t)as_int(size)));
        free_value(count);
        free_value(size);
        return result;
    }

    if (token_equals(&callee->token, "sizeof") || token_equals(&callee->token, "tamano_de")) {
        RuntimeValue value;
        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "sizeof requires one argument");
            *ok = 0;
            return result;
        }
        if (expr->args[0] && expr->args[0]->kind == TRAZO_EXPR_TYPE) {
            result = value_int(sizeof_type(&expr->args[0]->type));
            return result;
        }
        value = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        switch (value.kind) {
            case RV_BOOL: result = value_int(sizeof(int)); break;
            case RV_INT: result = value_int(sizeof(int64_t)); break;
            case RV_FLOAT: result = value_int(sizeof(double)); break;
            case RV_PTR: result = value_int(sizeof(void*)); break;
            case RV_DEC32: result = value_int(sizeof(decimal32)); break;
            case RV_DEC64: result = value_int(sizeof(decimal64)); break;
            case RV_DEC128: result = value_int(sizeof(decimal128)); break;
            case RV_STRING: result = value_int(value.as.string_value ? (int64_t)(strlen(value.as.string_value) + 1) : 0); break;
            default: result = value_int(sizeof(RuntimeValue)); break;
        }
        free_value(value);
        return result;
    }

    

    if (token_equals(&callee->token, "print") || token_equals(&callee->token, "imprimir")) {
        for (size_t i = 0; i < expr->arg_count; i++) {
            RuntimeValue arg = eval_expr(rt, expr->args[i], ok);
            if (!*ok) return result;
            if (i > 0) printf(" ");
            print_value(arg);
            free_value(arg);
        }
        printf("\n");
        return value_void();
    }

    if (token_equals(&callee->token, "read_line") || token_equals(&callee->token, "leer_linea")) {
        if (expr->arg_count != 0) {
            print_runtime_error(&callee->token, "read_line takes no arguments");
            *ok = 0;
            return result;
        }
        return value_string_owned(read_line_stdin());
    }

    if (token_equals(&callee->token, "input") || token_equals(&callee->token, "entrada")) {
        if (expr->arg_count > 1) {
            print_runtime_error(&callee->token, "input takes at most one argument");
            *ok = 0;
            return result;
        }
        if (expr->arg_count == 1) {
            RuntimeValue prompt = eval_expr(rt, expr->args[0], ok);
            if (!*ok) return result;
            if (prompt.kind != RV_STRING || !prompt.as.string_value) {
                free_value(prompt);
                print_runtime_error(&callee->token, "input prompt must be a string");
                *ok = 0;
                return result;
            }
            printf("%s", prompt.as.string_value);
            fflush(stdout);
            free_value(prompt);
        }
        return value_string_owned(read_line_stdin());
    }

    if (token_equals(&callee->token, "read_file") || token_equals(&callee->token, "leer_archivo")) {
        RuntimeValue path;
        char* data;

        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "read_file requires one path argument");
            *ok = 0;
            return result;
        }

        path = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        if (path.kind != RV_STRING || !path.as.string_value) {
            free_value(path);
            print_runtime_error(&callee->token, "read_file path must be a string");
            *ok = 0;
            return result;
        }

        data = read_entire_file(path.as.string_value);
        free_value(path);
        if (!data) {
            print_runtime_error(&callee->token, "could not read file");
            *ok = 0;
            return result;
        }

        return value_string_owned(data);
    }

    if (token_equals(&callee->token, "write_file") || token_equals(&callee->token, "escribir_archivo") ||
        token_equals(&callee->token, "append_file") || token_equals(&callee->token, "anexar_archivo")) {
        RuntimeValue path;
        RuntimeValue data;
        int append = token_equals(&callee->token, "append_file") || token_equals(&callee->token, "anexar_archivo");

        if (expr->arg_count != 2) {
            print_runtime_error(&callee->token, "write_file/append_file requires path and data arguments");
            *ok = 0;
            return result;
        }

        path = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        data = eval_expr(rt, expr->args[1], ok);
        if (!*ok) {
            free_value(path);
            return result;
        }

        if (path.kind != RV_STRING || data.kind != RV_STRING) {
            free_value(path);
            free_value(data);
            print_runtime_error(&callee->token, "file path and data must be strings");
            *ok = 0;
            return result;
        }

        result = value_bool(write_entire_file(path.as.string_value, data.as.string_value, append ? "ab" : "wb"));
        free_value(path);
        free_value(data);
        return result;
    }

    if (token_equals(&callee->token, "file_exists") || token_equals(&callee->token, "archivo_existe")) {
        RuntimeValue path;
        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "file_exists requires one path argument");
            *ok = 0;
            return result;
        }
        path = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        if (path.kind != RV_STRING) {
            free_value(path);
            print_runtime_error(&callee->token, "file_exists path must be a string");
            *ok = 0;
            return result;
        }
        result = value_bool(file_exists(path.as.string_value));
        free_value(path);
        return result;
    }

    if (token_equals(&callee->token, "len") || token_equals(&callee->token, "longitud")) {
        RuntimeValue value;
        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "len requires one argument");
            *ok = 0;
            return result;
        }
        value = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        if (value.kind == RV_STRING) {
            result = value_int(value.as.string_value ? (int64_t)strlen(value.as.string_value) : 0);
        } else if (value.kind == RV_ARRAY && value.as.array_value) {
            result = value_int((int64_t)value.as.array_value->count);
        } else {
            result = value_int(0);
        }
        free_value(value);
        return result;
    }

    if (token_equals(&callee->token, "make_struct") || token_equals(&callee->token, "nuevo_estructura")) {
        if (expr->arg_count != 0) {
            print_runtime_error(&callee->token, "make_struct takes no arguments");
            *ok = 0;
            return result;
        }
        RuntimeStruct* s = (RuntimeStruct*)trazo_calloc(1, sizeof(RuntimeStruct));
        if (!s) {
            print_runtime_error(&callee->token, "out of memory creating struct");
            *ok = 0;
            return result;
        }
        return value_struct_new(s);
    }

    if (token_equals(&callee->token, "push") || token_equals(&callee->token, "empujar")) {
        if (expr->arg_count != 2) {
            print_runtime_error(&callee->token, "push requires (arrayIdentifier, value)");
            *ok = 0;
            return result;
        }
        TrazoExpr* target = expr->args[0];
        if (!target || target->kind != TRAZO_EXPR_IDENTIFIER) {
            print_runtime_error(&callee->token, "push first argument must be an identifier");
            *ok = 0;
            return result;
        }
        RuntimeVar* var = find_var(rt, &target->token);
        if (!var || var->value.kind != RV_ARRAY || !var->value.as.array_value) {
            print_runtime_error(&callee->token, "push target must be an array variable");
            *ok = 0;
            return result;
        }

        RuntimeValue val = eval_expr(rt, expr->args[1], ok);
        if (!*ok) return result;

        RuntimeArray* arr = var->value.as.array_value;
        size_t new_count = arr->count + 1;
        RuntimeValue* new_items = (RuntimeValue*)trazo_realloc(arr->items, new_count * sizeof(RuntimeValue));
        if (!new_items) {
            free_value(val);
            print_runtime_error(&callee->token, "out of memory in push");
            *ok = 0;
            return result;
        }
        arr->items = new_items;
        arr->items[new_count - 1] = copy_value(val);
        arr->count = new_count;
        free_value(val);
        return value_int((int64_t)new_count);
    }

    if (token_equals(&callee->token, "pop") || token_equals(&callee->token, "sacar")) {
        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "pop requires (arrayIdentifier)");
            *ok = 0;
            return result;
        }
        TrazoExpr* target = expr->args[0];
        if (!target || target->kind != TRAZO_EXPR_IDENTIFIER) {
            print_runtime_error(&callee->token, "pop first argument must be an identifier");
            *ok = 0;
            return result;
        }
        RuntimeVar* var = find_var(rt, &target->token);
        if (!var || var->value.kind != RV_ARRAY || !var->value.as.array_value) {
            print_runtime_error(&callee->token, "pop target must be an array variable");
            *ok = 0;
            return result;
        }

        RuntimeArray* arr = var->value.as.array_value;
        if (arr->count == 0) {
            print_runtime_error(&callee->token, "pop from empty array");
            *ok = 0;
            return result;
        }
        size_t idx = arr->count - 1;
        RuntimeValue popped = copy_value(arr->items[idx]);

        size_t new_count = arr->count - 1;
        if (new_count == 0) {
            trazo_free(arr->items);
            arr->items = NULL;
            arr->count = 0;
        } else {
            RuntimeValue* new_items = (RuntimeValue*)trazo_realloc(arr->items, new_count * sizeof(RuntimeValue));
            if (new_items) arr->items = new_items;
            arr->count = new_count;
        }

        return popped;
    }

    if (token_equals(&callee->token, "set") || token_equals(&callee->token, "asignar")) {
        if (expr->arg_count != 3) {
            print_runtime_error(&callee->token, "set requires (arrayIdentifier, index, value)");
            *ok = 0;
            return result;
        }
        TrazoExpr* target = expr->args[0];
        if (!target || target->kind != TRAZO_EXPR_IDENTIFIER) {
            print_runtime_error(&callee->token, "set first argument must be an identifier");
            *ok = 0;
            return result;
        }
        RuntimeVar* var = find_var(rt, &target->token);
        if (!var || var->value.kind != RV_ARRAY || !var->value.as.array_value) {
            print_runtime_error(&callee->token, "set target must be an array variable");
            *ok = 0;
            return result;
        }

        RuntimeValue idxv = eval_expr(rt, expr->args[1], ok);
        if (!*ok) return result;
        size_t idx = (size_t)as_int(idxv);
        free_value(idxv);

        RuntimeValue val = eval_expr(rt, expr->args[2], ok);
        if (!*ok) return result;

        RuntimeArray* arr = var->value.as.array_value;
        if (idx >= arr->count) {
            free_value(val);
            print_runtime_error(&callee->token, "array index out of bounds in set");
            *ok = 0;
            return result;
        }

        free_value(arr->items[idx]);
        arr->items[idx] = copy_value(val);
        free_value(val);
        return value_void();
    }

    if (token_equals(&callee->token, "memcpy") || token_equals(&callee->token, "copiar_mem")) {
        RuntimeValue dst;
        RuntimeValue src;
        RuntimeValue size;
        if (!require_unsafe(rt, &callee->token, "memcpy")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 3) {
            print_runtime_error(&callee->token, "memcpy requires dst, src, size");
            *ok = 0;
            return result;
        }
        dst = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        src = eval_expr(rt, expr->args[1], ok);
        if (!*ok) { free_value(dst); return result; }
        size = eval_expr(rt, expr->args[2], ok);
        if (!*ok) { free_value(dst); free_value(src); return result; }
        if (dst.kind == RV_PTR && src.kind == RV_PTR) memcpy(dst.as.ptr_value, src.as.ptr_value, (size_t)as_int(size));
        result = dst.kind == RV_PTR ? value_ptr(dst.as.ptr_value) : value_null();
        free_value(dst);
        free_value(src);
        free_value(size);
        return result;
    }

    if (token_equals(&callee->token, "memset") || token_equals(&callee->token, "llenar_mem")) {
        RuntimeValue dst;
        RuntimeValue byte_value;
        RuntimeValue size;
        if (!require_unsafe(rt, &callee->token, "memset")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 3) {
            print_runtime_error(&callee->token, "memset requires dst, byte, size");
            *ok = 0;
            return result;
        }
        dst = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        byte_value = eval_expr(rt, expr->args[1], ok);
        if (!*ok) { free_value(dst); return result; }
        size = eval_expr(rt, expr->args[2], ok);
        if (!*ok) { free_value(dst); free_value(byte_value); return result; }
        if (dst.kind == RV_PTR) memset(dst.as.ptr_value, (int)as_int(byte_value), (size_t)as_int(size));
        result = dst.kind == RV_PTR ? value_ptr(dst.as.ptr_value) : value_null();
        free_value(dst);
        free_value(byte_value);
        free_value(size);
        return result;
    }

    if (token_equals(&callee->token, "memcmp") || token_equals(&callee->token, "comparar_mem")) {
        RuntimeValue a;
        RuntimeValue b;
        RuntimeValue size;
        if (!require_unsafe(rt, &callee->token, "memcmp")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 3) {
            print_runtime_error(&callee->token, "memcmp requires left, right, size");
            *ok = 0;
            return result;
        }
        a = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        b = eval_expr(rt, expr->args[1], ok);
        if (!*ok) { free_value(a); return result; }
        size = eval_expr(rt, expr->args[2], ok);
        if (!*ok) { free_value(a); free_value(b); return result; }
        if (a.kind == RV_PTR && b.kind == RV_PTR) {
            result = value_int(memcmp(a.as.ptr_value, b.as.ptr_value, (size_t)as_int(size)));
        } else {
            result = value_int(0);
        }
        free_value(a);
        free_value(b);
        free_value(size);
        return result;
    }

    if (token_equals(&callee->token, "realloc") || token_equals(&callee->token, "reasignar")) {
        RuntimeValue ptr;
        RuntimeValue size;
        if (!require_unsafe(rt, &callee->token, "realloc")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 2) {
            print_runtime_error(&callee->token, "realloc requires pointer and size arguments");
            *ok = 0;
            return result;
        }
        ptr = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        size = eval_expr(rt, expr->args[1], ok);
        if (!*ok) {
            free_value(ptr);
            return result;
        }
        result = value_ptr(trazo_realloc(ptr.kind == RV_PTR ? ptr.as.ptr_value : NULL, (size_t)as_int(size)));
        free_value(ptr);
        free_value(size);
        return result;
    }

    if (token_equals(&callee->token, "free") || token_equals(&callee->token, "liberar") ||
        token_equals(&callee->token, "delete") || token_equals(&callee->token, "eliminar")) {
        RuntimeValue ptr;
        if (!require_unsafe(rt, &callee->token, "free/delete")) {
            *ok = 0;
            return result;
        }
        if (expr->arg_count != 1) {
            print_runtime_error(&callee->token, "free/delete requires one pointer argument");
            *ok = 0;
            return result;
        }
        ptr = eval_expr(rt, expr->args[0], ok);
        if (!*ok) return result;
        if (ptr.kind == RV_PTR) trazo_free(ptr.as.ptr_value);
        free_value(ptr);
        return value_void();
    }

    user_func = find_func(rt, &callee->token);
    if (user_func) {
        if (expr->arg_count != user_func->param_count) {
            print_runtime_error(&callee->token, "wrong argument count");
            *ok = 0;
            return result;
        }

        enter_scope(rt);
        for (size_t i = 0; i < expr->arg_count; i++) {
            RuntimeValue arg = eval_expr(rt, expr->args[i], ok);
            TrazoToken param_token;
            if (!*ok) {
                leave_scope(rt);
                return result;
            }
            memset(&param_token, 0, sizeof(param_token));
            param_token.start = user_func->param_names[i];
            param_token.length = strlen(user_func->param_names[i]);
            if (!define_var(rt, &param_token, arg)) {
                free_value(arg);
                leave_scope(rt);
                *ok = 0;
                return result;
            }
            free_value(arg);
        }

        free_value(rt->return_value);
        rt->return_value = value_void();
        if (user_func->is_unsafe) rt->unsafe_depth++;
        if (exec_stmt(rt, user_func->body) == EXEC_ERROR) {
            if (user_func->is_unsafe && rt->unsafe_depth > 0) rt->unsafe_depth--;
            leave_scope(rt);
            *ok = 0;
            return result;
        }
        if (user_func->is_unsafe && rt->unsafe_depth > 0) rt->unsafe_depth--;
        result = copy_value(rt->return_value);
        leave_scope(rt);
        return result;
    }

    {
        TrazoStmt* extern_decl = find_extern(rt, &callee->token);
        if (extern_decl) {
            return eval_extern_call(rt, extern_decl, expr, ok);
        }
    }

    print_runtime_error(&callee->token, "unknown function");
    *ok = 0;
    return result;
}

static RuntimeValue eval_decimal_binary(RuntimeValue left, RuntimeValue right, TrazoTokenType op, const TrazoToken* token, int* ok) {
    char* left_text = decimal_value_to_string(left);
    char* right_text = decimal_value_to_string(right);
    int digits = token_decimal_digits(left_text ? left_text : "0") + token_decimal_digits(right_text ? right_text : "0") + 8;
    decContext ctx = decimal_context(digits);
    decNumber* a = decimal_alloc_digits(digits);
    decNumber* b = decimal_alloc_digits(digits);
    decNumber* r = decimal_alloc_digits(digits);
    RuntimeValue result = value_void();

    if (!a || !b || !r || !left_text || !right_text) {
        *ok = 0;
        print_runtime_error(token, "decimal allocation failed");
        goto done;
    }

    decNumberFromString(a, left_text, &ctx);
    decNumberFromString(b, right_text, &ctx);

    switch (op) {
        case TRAZO_TOKEN_PLUS:
            decNumberAdd(r, a, b, &ctx);
            result.kind = RV_DECIMAL;
            result.as.decimal_value = r;
            r = NULL;
            break;
        case TRAZO_TOKEN_MINUS:
            decNumberSubtract(r, a, b, &ctx);
            result.kind = RV_DECIMAL;
            result.as.decimal_value = r;
            r = NULL;
            break;
        case TRAZO_TOKEN_STAR:
            decNumberMultiply(r, a, b, &ctx);
            result.kind = RV_DECIMAL;
            result.as.decimal_value = r;
            r = NULL;
            break;
        case TRAZO_TOKEN_SLASH:
            if (decNumberIsZero(b)) {
                *ok = 0;
                print_runtime_error(token, "division by zero");
                break;
            }
            decNumberDivide(r, a, b, &ctx);
            result.kind = RV_DECIMAL;
            result.as.decimal_value = r;
            r = NULL;
            break;
        case TRAZO_TOKEN_EQUAL_EQUAL:
        case TRAZO_TOKEN_BANG_EQUAL:
        case TRAZO_TOKEN_LESS:
        case TRAZO_TOKEN_LESS_EQUAL:
        case TRAZO_TOKEN_GREATER:
        case TRAZO_TOKEN_GREATER_EQUAL:
            decNumberCompare(r, a, b, &ctx);
            {
                int cmp = decNumberIsNegative(r) ? -1 : (decNumberIsZero(r) ? 0 : 1);
                switch (op) {
                    case TRAZO_TOKEN_EQUAL_EQUAL: result = value_bool(cmp == 0); break;
                    case TRAZO_TOKEN_BANG_EQUAL: result = value_bool(cmp != 0); break;
                    case TRAZO_TOKEN_LESS: result = value_bool(cmp < 0); break;
                    case TRAZO_TOKEN_LESS_EQUAL: result = value_bool(cmp <= 0); break;
                    case TRAZO_TOKEN_GREATER: result = value_bool(cmp > 0); break;
                    case TRAZO_TOKEN_GREATER_EQUAL: result = value_bool(cmp >= 0); break;
                    default: break;
                }
            }
            break;
        default:
            *ok = 0;
            print_runtime_error(token, "unsupported decimal operator");
            break;
    }

done:
    trazo_free(left_text);
    trazo_free(right_text);
    trazo_free(a);
    trazo_free(b);
    trazo_free(r);
    return result;
}

static RuntimeValue eval_binary(Runtime* rt, TrazoExpr* expr, int* ok) {
    RuntimeValue left;
    RuntimeValue right;
    RuntimeValue result = value_void();
    int use_float;

    if (expr->token.type == TRAZO_TOKEN_AMP_AMP) {
        left = eval_expr(rt, expr->left, ok);
        if (!*ok) return result;
        if (!is_truthy(left)) {
            free_value(left);
            return value_bool(0);
        }
        free_value(left);
        right = eval_expr(rt, expr->right, ok);
        if (!*ok) return result;
        result = value_bool(is_truthy(right));
        free_value(right);
        return result;
    }

    if (expr->token.type == TRAZO_TOKEN_PIPE_PIPE) {
        left = eval_expr(rt, expr->left, ok);
        if (!*ok) return result;
        if (is_truthy(left)) {
            free_value(left);
            return value_bool(1);
        }
        free_value(left);
        right = eval_expr(rt, expr->right, ok);
        if (!*ok) return result;
        result = value_bool(is_truthy(right));
        free_value(right);
        return result;
    }

    left = eval_expr(rt, expr->left, ok);
    if (!*ok) return result;
    right = eval_expr(rt, expr->right, ok);
    if (!*ok) {
        free_value(left);
        return result;
    }

    if (is_decimal_value(left) || is_decimal_value(right)) {
        result = eval_decimal_binary(left, right, expr->token.type, &expr->token, ok);
        free_value(left);
        free_value(right);
        return result;
    }

    use_float = left.kind == RV_FLOAT || right.kind == RV_FLOAT;

    switch (expr->token.type) {
        case TRAZO_TOKEN_PLUS:
            result = use_float ? value_float(as_float(left) + as_float(right)) : value_int(as_int(left) + as_int(right));
            break;
        case TRAZO_TOKEN_MINUS:
            result = use_float ? value_float(as_float(left) - as_float(right)) : value_int(as_int(left) - as_int(right));
            break;
        case TRAZO_TOKEN_STAR:
            result = use_float ? value_float(as_float(left) * as_float(right)) : value_int(as_int(left) * as_int(right));
            break;
        case TRAZO_TOKEN_SLASH:
            if (as_float(right) == 0.0) {
                print_runtime_error(&expr->token, "division by zero");
                *ok = 0;
                break;
            }
            result = value_float(as_float(left) / as_float(right));
            break;
        case TRAZO_TOKEN_PERCENT:
            if (as_int(right) == 0) {
                print_runtime_error(&expr->token, "modulo by zero");
                *ok = 0;
                break;
            }
            result = value_int(as_int(left) % as_int(right));
            break;
        case TRAZO_TOKEN_PIPE:
            result = value_int(as_int(left) | as_int(right));
            break;
        case TRAZO_TOKEN_AMPERSAND:
            result = value_int(as_int(left) & as_int(right));
            break;
        case TRAZO_TOKEN_CARET:
            result = value_int(as_int(left) ^ as_int(right));
            break;
        case TRAZO_TOKEN_LESS_LESS:
            result = value_int(as_int(left) << as_int(right));
            break;
        case TRAZO_TOKEN_GREATER_GREATER:
            result = value_int(as_int(left) >> as_int(right));
            break;
        case TRAZO_TOKEN_EQUAL_EQUAL:
            result = value_bool(as_float(left) == as_float(right));
            break;
        case TRAZO_TOKEN_BANG_EQUAL:
            result = value_bool(as_float(left) != as_float(right));
            break;
        case TRAZO_TOKEN_LESS:
            result = value_bool(as_float(left) < as_float(right));
            break;
        case TRAZO_TOKEN_LESS_EQUAL:
            result = value_bool(as_float(left) <= as_float(right));
            break;
        case TRAZO_TOKEN_GREATER:
            result = value_bool(as_float(left) > as_float(right));
            break;
        case TRAZO_TOKEN_GREATER_EQUAL:
            result = value_bool(as_float(left) >= as_float(right));
            break;
        default:
            print_runtime_error(&expr->token, "unsupported binary operator");
            *ok = 0;
            break;
    }

    free_value(left);
    free_value(right);
    return result;
}

static int64_t utf8_to_codepoint(const char* bytes, size_t length) {
    if (length == 0) return 0;
    if (bytes[0] == '\\') {
        if (length < 2) return 0;
        switch (bytes[1]) {
            case 'n': return '\n';
            case 'r': return '\r';
            case 't': return '\t';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            case '0': return '\0';
            default: return bytes[1];
        }
    }

    unsigned char b0 = (unsigned char)bytes[0];
    if ((b0 & 0x80) == 0) {
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && length >= 2) {
        return ((b0 & 0x1F) << 6) | ((unsigned char)bytes[1] & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && length >= 3) {
        return ((b0 & 0x0F) << 12) |
               (((unsigned char)bytes[1] & 0x3F) << 6) |
               ((unsigned char)bytes[2] & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && length >= 4) {
        return ((b0 & 0x07) << 18) |
               (((unsigned char)bytes[1] & 0x3F) << 12) |
               (((unsigned char)bytes[2] & 0x3F) << 6) |
               ((unsigned char)bytes[3] & 0x3F);
    }
    return 0;
}

static RuntimeValue eval_expr(Runtime* rt, TrazoExpr* expr, int* ok) {
    RuntimeValue value = value_void();
    RuntimeVar* var;

    if (!expr) return value;

    switch (expr->kind) {
        case TRAZO_EXPR_LITERAL:
            switch (expr->token.type) {
                case TRAZO_TOKEN_INT_LITERAL: {
                    char* text = token_text(&expr->token);
                    value = value_int(text ? strtoll(text, NULL, 0) : 0);
                    trazo_free(text);
                    return value;
                }
                case TRAZO_TOKEN_FLOAT_LITERAL: {
                    char* text = token_text(&expr->token);
                    value = value_float(text ? strtod(text, NULL) : 0.0);
                    trazo_free(text);
                    return value;
                }
                case TRAZO_TOKEN_STRING_LITERAL:
                    return value_string_from_token(&expr->token);
                case TRAZO_TOKEN_CHAR_LITERAL: {
                    size_t length = expr->token.length >= 2 ? expr->token.length - 2 : 0;
                    const char* bytes = expr->token.start + 1;
                    return value_int(utf8_to_codepoint(bytes, length));
                }
                case TRAZO_TOKEN_TRUE:
                    return value_bool(1);
                case TRAZO_TOKEN_FALSE:
                    return value_bool(0);
                case TRAZO_TOKEN_NULL:
                    return value_null();
                default:
                    return value_void();
            }

        case TRAZO_EXPR_IDENTIFIER:
            var = find_var(rt, &expr->token);
            if (!var) {
                print_runtime_error(&expr->token, "undefined variable");
                *ok = 0;
                return value;
            }
            return copy_value(var->value);

        case TRAZO_EXPR_TYPE:
            print_runtime_error(&expr->token, "type name cannot be used as an expression");
            *ok = 0;
            return value;

        case TRAZO_EXPR_ASSIGN:
            if (!expr->left) {
                print_runtime_error(&expr->token, "assignment target missing");
                *ok = 0;
                return value;
            }

            if (expr->left->kind == TRAZO_EXPR_IDENTIFIER) {
                var = find_var(rt, &expr->left->token);
                if (!var) {
                    print_runtime_error(&expr->left->token, "undefined variable");
                    *ok = 0;
                    return value;
                }
                value = eval_expr(rt, expr->right, ok);
                if (*ok) assign_var(var, value);
                return value;
            } else if (expr->left->kind == TRAZO_EXPR_UNARY && expr->left->token.type == TRAZO_TOKEN_STAR) {
                RuntimeValue* address;
                if (!require_unsafe(rt, &expr->left->token, "pointer indirection assignment")) {
                    *ok = 0;
                    return value;
                }
                address = eval_address(rt, expr->left, ok);
                if (!*ok || !address) return value;
                value = eval_expr(rt, expr->right, ok);
                if (*ok) {
                    free_value(*address);
                    *address = copy_value(value);
                }
                return value;
            } else if (expr->left->kind == TRAZO_EXPR_INDEX) {
                RuntimeValue* address = eval_address(rt, expr->left, ok);
                if (!*ok || !address) return value;
                value = eval_expr(rt, expr->right, ok);
                if (*ok) {
                    free_value(*address);
                    *address = copy_value(value);
                }
                return value;
            } else if (expr->left->kind == TRAZO_EXPR_FIELD) {
                /* Assignment to struct field: only support when target is an identifier */
                TrazoExpr* field = expr->left;
                if (!field->left || field->left->kind != TRAZO_EXPR_IDENTIFIER) {
                    print_runtime_error(&expr->token, "field assignment target must be variable.field");
                    *ok = 0;
                    return value;
                }
                RuntimeVar* base = find_var(rt, &field->left->token);
                if (!base) {
                    print_runtime_error(&field->left->token, "undefined variable");
                    *ok = 0;
                    return value;
                }
                if (base->value.kind != RV_STRUCT || !base->value.as.ptr_value) {
                    print_runtime_error(&field->token, "field assignment target is not a struct");
                    *ok = 0;
                    return value;
                }

                RuntimeStruct* s = (RuntimeStruct*)base->value.as.ptr_value;
                const char* fname = field->token.start;
                size_t flen = field->token.length;
                RuntimeStructType* type = s->type_name ? find_struct_type(rt, s->type_name, strlen(s->type_name)) : NULL;

                value = eval_expr(rt, expr->right, ok);
                if (!*ok) return value;

                if (type && !struct_type_has_field(type, fname, flen)) {
                    free_value(value);
                    print_runtime_error(&field->token, "unknown field for struct type");
                    *ok = 0;
                    return value;
                }

                /* find existing field */
                for (size_t i = 0; i < s->count; i++) {
                    if (strlen(s->names[i]) == flen && memcmp(s->names[i], fname, flen) == 0) {
                        free_value(s->values[i]);
                        s->values[i] = copy_value(value);
                        return value;
                    }
                }

                /* append new field */
                size_t next = s->count + 1;
                if (next > s->capacity) {
                    size_t nc = s->capacity < 8 ? 8 : s->capacity * 2;
                    char** nn = (char**)trazo_realloc(s->names, nc * sizeof(char*));
                    RuntimeValue* nv = (RuntimeValue*)trazo_realloc(s->values, nc * sizeof(RuntimeValue));
                    if (!nn || !nv) {
                        free_value(value);
                        print_runtime_error(&field->token, "out of memory setting struct field");
                        *ok = 0;
                        return value;
                    }
                    s->names = nn;
                    s->values = nv;
                    s->capacity = nc;
                }
                s->names[s->count] = strdup_len(fname, flen);
                s->values[s->count] = copy_value(value);
                s->count++;
                return value;
            }

            print_runtime_error(&expr->token, "assignment target must be a variable or struct field");
            *ok = 0;
            return value;

        case TRAZO_EXPR_BINARY:
            return eval_binary(rt, expr, ok);

        case TRAZO_EXPR_UNARY:
            switch (expr->token.type) {
                case TRAZO_TOKEN_BANG:
                {
                    RuntimeValue value = eval_expr(rt, expr->right, ok);
                    if (!*ok) return value;
                    int truthy = is_truthy(value);
                    free_value(value);
                    return value_bool(!truthy);
                }
                case TRAZO_TOKEN_MINUS:
                {
                    RuntimeValue value = eval_expr(rt, expr->right, ok);
                    if (!*ok) return value;
                    if (is_decimal_value(value)) {
                        RuntimeValue zero = value_decimal_from_string("0");
                        RuntimeValue neg = eval_decimal_binary(zero, value, TRAZO_TOKEN_MINUS, &expr->token, ok);
                        free_value(zero);
                        free_value(value);
                        return neg;
                    } else if (value.kind == RV_FLOAT) {
                        value.as.float_value = -value.as.float_value;
                    } else {
                        value = value_int(-as_int(value));
                    }
                    return value;
                }
                case TRAZO_TOKEN_TILDE:
                {
                    RuntimeValue value = eval_expr(rt, expr->right, ok);
                    if (!*ok) return value;
                    value = value_int(~as_int(value));
                    return value;
                }
                case TRAZO_TOKEN_AMPERSAND:
                {
                    RuntimeValue* address;
                    if (!require_unsafe(rt, &expr->token, "address-of")) {
                        *ok = 0;
                        return value_void();
                    }
                    address = eval_address(rt, expr->right, ok);
                    return *ok ? value_ptr(address) : value_void();
                }
                case TRAZO_TOKEN_STAR:
                {
                    RuntimeValue* address;
                    if (!require_unsafe(rt, &expr->token, "pointer dereference")) {
                        *ok = 0;
                        return value_void();
                    }
                    address = eval_address(rt, expr, ok);
                    return (*ok && address) ? copy_value(*address) : value_void();
                }
                default:
                    print_runtime_error(&expr->token, "unsafe pointer unary operator is parsed but not interpreted yet");
                    *ok = 0;
                    return value_void();
            }

        case TRAZO_EXPR_CALL:
            return eval_call(rt, expr, ok);

        case TRAZO_EXPR_CAST:
        {
            RuntimeValue value = eval_expr(rt, expr->right, ok);
            if (!*ok) return value;
            if (expr->type.kind == TRAZO_TYPE_UNKNOWN && !expr->type.name) {
                return value;
            }
            return cast_runtime_value(value, expr->type);
        }

        case TRAZO_EXPR_ARRAY:
        {
            RuntimeArray* array = (RuntimeArray*)trazo_calloc(1, sizeof(RuntimeArray));
            if (!array) {
                *ok = 0;
                return value_null();
            }
            array->count = expr->arg_count;
            array->items = (RuntimeValue*)trazo_calloc(array->count, sizeof(RuntimeValue));
            if (!array->items) {
                trazo_free(array);
                *ok = 0;
                return value_null();
            }
            for (size_t i = 0; i < array->count; i++) {
                array->items[i] = eval_expr(rt, expr->args[i], ok);
                if (!*ok) {
                    RuntimeValue av = value_array(array);
                    free_value(av);
                    return value_null();
                }
            }
            return value_array(array);
        }

        case TRAZO_EXPR_INDEX:
        {
            RuntimeValue target = eval_expr(rt, expr->left, ok);
            RuntimeValue idx;
            size_t index;
            if (!*ok) return value_null();
            idx = eval_expr(rt, expr->right, ok);
            if (!*ok) {
                free_value(target);
                return value_null();
            }
            index = (size_t)as_int(idx);
            free_value(idx);
            if (target.kind != RV_ARRAY || !target.as.array_value || index >= target.as.array_value->count) {
                free_value(target);
                print_runtime_error(&expr->token, "array index out of bounds");
                *ok = 0;
                return value_null();
            }
            value = copy_value(target.as.array_value->items[index]);
            free_value(target);
            return value;
        }

        case TRAZO_EXPR_FIELD:
        {
            RuntimeValue target = eval_expr(rt, expr->left, ok);
            if (!*ok) return value;
            if (target.kind == RV_STRUCT && target.as.ptr_value) {
                RuntimeStruct* s = (RuntimeStruct*)target.as.ptr_value;
                const char* fname = expr->token.start;
                size_t flen = expr->token.length;
                RuntimeStructType* type = s->type_name ? find_struct_type(rt, s->type_name, strlen(s->type_name)) : NULL;
                for (size_t i = 0; i < s->count; i++) {
                    if (strlen(s->names[i]) == flen && memcmp(s->names[i], fname, flen) == 0) {
                        RuntimeValue res = copy_value(s->values[i]);
                        free_value(target);
                        return res;
                    }
                }
                free_value(target);
                if (type) {
                    print_runtime_error(&expr->token, "unknown struct field");
                } else {
                    print_runtime_error(&expr->token, "unknown struct field");
                }
                *ok = 0;
                return value_null();
            }

            free_value(target);
            print_runtime_error(&expr->token, "field access on non-struct value");
            *ok = 0;
            return value_null();
        }
    }

    return value;
}

static ExecResult exec_block(Runtime* rt, TrazoStmt* block) {
    ExecResult result = EXEC_OK;
    enter_scope(rt);
    for (size_t i = 0; i < block->child_count; i++) {
        result = exec_stmt(rt, block->children[i]);
        if (result != EXEC_OK) break;
    }
    leave_scope(rt);
    return result;
}

static ExecResult exec_stmt(Runtime* rt, TrazoStmt* stmt) {
    int ok = 1;
    RuntimeValue value;

    if (!stmt) return EXEC_OK;

    switch (stmt->kind) {
        case TRAZO_STMT_EXPR:
            value = eval_expr(rt, stmt->expr, &ok);
            free_value(value);
            return ok ? EXEC_OK : EXEC_ERROR;

        case TRAZO_STMT_VAR:
            value = value_null();
            if ((stmt->type.kind == TRAZO_TYPE_DECIMAL || stmt->type.kind == TRAZO_TYPE_DEC32 ||
                 stmt->type.kind == TRAZO_TYPE_DEC64 || stmt->type.kind == TRAZO_TYPE_DEC128) &&
                stmt->expr && stmt->expr->kind == TRAZO_EXPR_LITERAL &&
                (stmt->expr->token.type == TRAZO_TOKEN_INT_LITERAL ||
                 stmt->expr->token.type == TRAZO_TOKEN_FLOAT_LITERAL)) {
                char* text = token_text(&stmt->expr->token);
                switch (stmt->type.kind) {
                    case TRAZO_TYPE_DECIMAL: value = value_decimal_from_string(text ? text : "0"); break;
                    case TRAZO_TYPE_DEC32: value = value_dec32_from_string(text ? text : "0"); break;
                    case TRAZO_TYPE_DEC64: value = value_dec64_from_string(text ? text : "0"); break;
                    case TRAZO_TYPE_DEC128: value = value_dec128_from_string(text ? text : "0"); break;
                    default: break;
                }
                trazo_free(text);
            } else if (stmt->expr) {
                value = eval_expr(rt, stmt->expr, &ok);
            } else if (stmt->type.kind == TRAZO_TYPE_STRUCT) {
                RuntimeStruct* s = (RuntimeStruct*)trazo_calloc(1, sizeof(RuntimeStruct));
                if (!s) {
                    print_runtime_error(&stmt->token, "out of memory creating struct");
                    return EXEC_ERROR;
                }
                value = value_struct_new(s);
            }
            if (!ok) {
                free_value(value);
                return EXEC_ERROR;
            }
            if (stmt->type.kind == TRAZO_TYPE_DECIMAL || stmt->type.kind == TRAZO_TYPE_DEC32 ||
                stmt->type.kind == TRAZO_TYPE_DEC64 || stmt->type.kind == TRAZO_TYPE_DEC128) {
                RuntimeValue coerced = coerce_to_declared_type(value, stmt->type.kind);
                free_value(value);
                value = coerced;
            }
            if (stmt->type.kind == TRAZO_TYPE_STRUCT && value.kind == RV_STRUCT && stmt->type.name) {
                RuntimeStruct* s = (RuntimeStruct*)value.as.ptr_value;
                if (s) {
                    if (!set_struct_type_name(rt, s, stmt->type.name, strlen(stmt->type.name))) {
                        free_value(value);
                        print_runtime_error(&stmt->token, "out of memory applying struct type");
                        return EXEC_ERROR;
                    }
                }
            }
            if (!define_var(rt, &stmt->token, value)) {
                free_value(value);
                print_runtime_error(&stmt->token, "could not define variable");
                return EXEC_ERROR;
            }
            free_value(value);
            return EXEC_OK;

        case TRAZO_STMT_BLOCK:
            return exec_block(rt, stmt);

        case TRAZO_STMT_IF:
            value = eval_expr(rt, stmt->expr, &ok);
            if (!ok) return EXEC_ERROR;
            ok = is_truthy(value);
            free_value(value);
            if (ok) return exec_stmt(rt, stmt->body);
            if (stmt->else_branch) return exec_stmt(rt, stmt->else_branch);
            return EXEC_OK;

        case TRAZO_STMT_WHILE:
            for (;;) {
                value = eval_expr(rt, stmt->expr, &ok);
                if (!ok) return EXEC_ERROR;
                ok = is_truthy(value);
                free_value(value);
                if (!ok) break;

                switch (exec_stmt(rt, stmt->body)) {
                    case EXEC_OK: break;
                    case EXEC_CONTINUE: continue;
                    case EXEC_BREAK: return EXEC_OK;
                    case EXEC_RETURN: return EXEC_RETURN;
                    case EXEC_ERROR: return EXEC_ERROR;
                }
            }
            return EXEC_OK;

        case TRAZO_STMT_FOR:
            enter_scope(rt);
            if (stmt->init) {
                ExecResult init_result = exec_stmt(rt, stmt->init);
                if (init_result != EXEC_OK) {
                    leave_scope(rt);
                    return init_result;
                }
            }
            for (;;) {
                if (stmt->expr) {
                    value = eval_expr(rt, stmt->expr, &ok);
                    if (!ok) {
                        leave_scope(rt);
                        return EXEC_ERROR;
                    }
                    ok = is_truthy(value);
                    free_value(value);
                    if (!ok) break;
                }

                switch (exec_stmt(rt, stmt->body)) {
                    case EXEC_OK: break;
                    case EXEC_CONTINUE: break;
                    case EXEC_BREAK:
                        leave_scope(rt);
                        return EXEC_OK;
                    case EXEC_RETURN:
                        leave_scope(rt);
                        return EXEC_RETURN;
                    case EXEC_ERROR:
                        leave_scope(rt);
                        return EXEC_ERROR;
                }

                if (stmt->expr2) {
                    value = eval_expr(rt, stmt->expr2, &ok);
                    free_value(value);
                    if (!ok) {
                        leave_scope(rt);
                        return EXEC_ERROR;
                    }
                }
            }
            leave_scope(rt);
            return EXEC_OK;

        case TRAZO_STMT_STRUCT:
            return register_struct_type(rt, stmt) ? EXEC_OK : EXEC_ERROR;

        case TRAZO_STMT_FUNC:
            return register_func(rt, stmt) ? EXEC_OK : EXEC_ERROR;

        case TRAZO_STMT_EXTERN:
            return register_extern(rt, stmt) ? EXEC_OK : EXEC_ERROR;

        case TRAZO_STMT_UNSAFE:
        {
            ExecResult unsafe_result;
            rt->unsafe_depth++;
            unsafe_result = exec_stmt(rt, stmt->body);
            if (rt->unsafe_depth > 0) rt->unsafe_depth--;
            return unsafe_result;
        }

        case TRAZO_STMT_RETURN:
            free_value(rt->return_value);
            rt->return_value = stmt->expr ? eval_expr(rt, stmt->expr, &ok) : value_void();
            return ok ? EXEC_RETURN : EXEC_ERROR;

        case TRAZO_STMT_BREAK:
            return EXEC_BREAK;

        case TRAZO_STMT_CONTINUE:
            return EXEC_CONTINUE;
    }

    return EXEC_OK;
}

int trazo_execute(TrazoProgram* program) {
    Runtime rt;
    ExecResult result = EXEC_OK;
    TrazoStmt* main_func;
    int exit_code = 0;

    if (!program) return 1;

    memset(&rt, 0, sizeof(rt));
    rt.return_value = value_void();

    for (size_t i = 0; i < program->count; i++) {
        if (program->statements[i]->kind == TRAZO_STMT_FUNC) {
            if (!register_func(&rt, program->statements[i])) {
                trazo_free(rt.funcs);
                trazo_free(rt.externs);
                free_struct_types(&rt);
                return 1;
            }
        } else if (program->statements[i]->kind == TRAZO_STMT_EXTERN) {
            if (!register_extern(&rt, program->statements[i])) {
                trazo_free(rt.funcs);
                trazo_free(rt.externs);
                free_struct_types(&rt);
                return 1;
            }
        } else if (program->statements[i]->kind == TRAZO_STMT_STRUCT) {
            if (!register_struct_type(&rt, program->statements[i])) {
                trazo_free(rt.funcs);
                trazo_free(rt.externs);
                free_struct_types(&rt);
                return 1;
            }
        }
    }

    main_func = find_func_name(&rt, "main");
    if (!main_func) {
        fprintf(stderr, "Runtime error: missing main function\n");
        result = EXEC_ERROR;
    } else if (main_func->param_count != 0) {
        print_runtime_error(&main_func->token, "main must not take parameters");
        result = EXEC_ERROR;
    } else {
        free_value(rt.return_value);
        rt.return_value = value_void();
        if (main_func->is_unsafe) rt.unsafe_depth++;
        enter_scope(&rt);
        result = exec_stmt(&rt, main_func->body);
        leave_scope(&rt);
        if (main_func->is_unsafe && rt.unsafe_depth > 0) rt.unsafe_depth--;
        if (result == EXEC_RETURN) {
            exit_code = (int)as_int(rt.return_value);
            result = EXEC_OK;
        }
    }

    while (rt.var_count > 0) {
        RuntimeVar* var = &rt.vars[--rt.var_count];
        trazo_free(var->name);
        free_value(var->value);
    }
    trazo_free(rt.vars);
    trazo_free(rt.funcs);
    trazo_free(rt.externs);
    free_struct_types(&rt);
    free_value(rt.return_value);

    return result == EXEC_ERROR ? 1 : exit_code;
}
