# Trazo Syntax

This file describes the syntax supported by the current Trazo implementation.
The lexer accepts English and Spanish keywords for the same token, so both
styles can be mixed, although examples usually keep one style per file.

## Comments

Trazo supports:

- `//` for single-line comments
- `#` for single-line comments
- `/* ... */` for block comments
- `## ... ###` for hash block comments

## Whitespace

Whitespace and line breaks separate tokens and are otherwise ignored.
Statements normally end with `;`.

## Identifiers

Identifiers begin with a letter or underscore and may contain letters, digits,
and underscores:

```trazo
my_value
_nombre
ptr123
```

## Literals

- Integer literals: `42`, `0x10`, `0b1010`
- Float literals: `3.14`, `0.5`, `1.0e3`, `2.0f`
- Boolean literals: `true`, `false`
- String literals: `"hello"`
- Character literals: `'a'`
- Null: `null`

Spanish boolean and null literals are also accepted: `verdadero`, `falso`,
and `nulo`.

## Types and Declarations

Variable declarations use an explicit type:

```trazo
int x = 10;
string greeting = "hello";
int* ptr = malloc(16);
```

Supported built-in type keywords include:

- `int`, `i8`, `i16`, `i32`, `i64`
- `u8`, `u16`, `u32`, `u64`, `byte`
- `char`, `rune`
- `f32`, `f64`, `float`, `double`
- `bool`, `string`, `void`
- `decimal`, `dec32`, `dec64`, `dec128`
- `ptr`

Pointer types are written with `*`, such as `int*` or `float*`.
Declarations may also be prefixed with `volatile`.

## Expressions

Trazo supports common expression forms:

- Assignment: `name = value`
- Binary arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Unary: `-`, `!`, `~`, `&`, `*`
- Parentheses: `(expr)`
- Casts: `expr as type`
- Field access: `value.field`
- Pointer field access: `value->field`
- Array indexing: `array[index]`
- Function calls: `fn(arg1, arg2)`

Address-of and pointer dereference operations require an unsafe context.

## Control Flow

### If Statement

```trazo
if (x > 0) {
    print("positive");
} elif (x == 0) {
    print("zero");
} else {
    print("negative");
}
```

### While Loop

```trazo
int i = 0;
while (i < 3) {
    print(i);
    i = i + 1;
}
```

### For Loop

```trazo
for (int i = 0; i < 3; i = i + 1) {
    print(i);
}
```

### Break and Continue

```trazo
for (int i = 0; i < 5; i = i + 1) {
    if (i == 2) {
        continue;
    }
    if (i == 4) {
        break;
    }
    print(i);
}
```

## Functions

Function declarations use `func`. A return type can be declared with `->`.

```trazo
func greet(string name) {
    print("Hello", name);
}

func main -> int {
    greet("Trazo");
    return 0;
}
```

The runtime looks for a zero-argument `main` function and executes it when
present.

## Unsafe Code

Unsafe operations must run inside `unsafe.MAX`.

An unsafe block:

```trazo
func main -> int {
    unsafe.MAX {
        int value = 42;
        int* ptr = &value;
        *ptr = 99;
    }
    return 0;
}
```

An unsafe function:

```trazo
unsafe.MAX func main -> int {
    int* buffer = malloc(64);
    memset(buffer, 0, 64);
    free(buffer);
    return 0;
}
```

The Spanish form `inseguro.MAX` is also recognized.

## Structs

Struct declarations use `struct`. Runtime struct values can be created with
`make_struct()` and then assigned fields.

```trazo
struct Profile {
    int age;
    string name;
}

func main -> int {
    Profile person = make_struct();
    person.age = 31;
    person.name = "Rosa";
    print(person.name, person.age);
    return 0;
}
```

## Arrays

Array literals use square brackets:

```trazo
func main -> int {
    int numbers = [1, 2, 3];
    print(len(numbers));
    push(numbers, 4);
    print(numbers[1]);
    return 0;
}
```

Current helpers include `push(array, value)`, `pop(array)`, `set(array, index,
value)`, and `len(array)`.

## Extern Declarations

Extern declarations can expose a small set of supported C library calls to
Trazo.

```trazo
extern int puts(string text);
extern int strlen(string text);

func main -> int {
    puts("called from Trazo");
    print(strlen("Trazo"));
    return 0;
}
```

The current runtime has special handling for functions such as `puts`,
`putchar`, `strlen`, `strcmp`, `getenv`, `atoi`, and `exit`.

## Builtin Helpers

Common runtime helpers include:

- `print(...)`
- `input(prompt?)`
- `read_line()`
- `read_file(path)`
- `write_file(path, data)`
- `append_file(path, data)`
- `file_exists(path)`
- `len(value)`
- `make_struct()`
- `push(array, value)`
- `pop(array)`
- `set(array, index, value)`
- `sizeof(type_or_value)`
- `malloc(size)`, `alloc(size)`, `calloc(count, size)`, `realloc(ptr, size)`
- `free(ptr)`, `delete(ptr)`
- `memcpy(dst, src, size)`, `memset(dst, byte, size)`, `memcmp(a, b, size)`

Manual memory, memory operations, and pointer operations require `unsafe.MAX`.

## Lexer Keyword Table

These are the English and Spanish keyword pairs currently listed in
`core/src/lexer.c`.

|    English    |       Spanish      |
| ------------- | ------------------ |
| `int`         | `entero`           |
| `string`      | `cadena`           |
| `float`       | `flotante`         |
| `bool`        | `booleano`         |
| `true`        | `verdadero`        |
| `false`       | `falso`            |
| `null`        | `nulo`             |
| `byte`        | `byte`             |
| `char`        | `caracter`         |
| `rune`        | `runa`             |
| `double`      | `doble`            |
| `void`        | `vacio`            |
| `i8`          | `e8`               |
| `i16`         | `e16`              |
| `i32`         | `e32`              |
| `i64`         | `e64`              |
| `u8`          | `u8`               |
| `u16`         | `u16`              |
| `u32`         | `u32`              |
| `u64`         | `u64`              |
| `f32`         | `f32`              |
| `f64`         | `f64`              |
| `decimal`     | `decimal`          |
| `dec32`       | `dec32`            |
| `dec64`       | `dec64`            |
| `dec128`      | `dec128`           |
| `ptr`         | `ptr`              |
| `if`          | `si`               |
| `elif`        | `sino_si`          |
| `else`        | `sino`             |
| `while`       | `mientras`         |
| `for`         | `para`             |
| `struct`      | `estructura`       |
| `func`        | `funcion`          |
| `extern`      | `externo`          |
| `return`      | `retornar`         |
| `break`       | `romper`           |
| `continue`    | `continuar`        |
| `malloc`      | `reservar`         |
| `alloc`       | `asignar_mem`      |
| `calloc`      | `asignar_cero`     |
| `realloc`     | `reasignar`        |
| `free`        | `liberar`          |
| `delete`      | `eliminar`         |
| `sizeof`      | `tamano_de`        |
| `print`       | `imprimir`         |
| `read_line`   | `leer_linea`       |
| `read_file`   | `leer_archivo`     |
| `write_file`  | `escribir_archivo` |
| `append_file` | `anexar_archivo`   |
| `file_exists` | `archivo_existe`   |
| `len`         | `longitud`         |
| `memcpy`      | `copimem`          |
| `memset`      | `llenmem`          |
| `memcmp`      | `comparmem`        |
| `volatile`    | `volatil`          |
| `as`          | `como`             |

`unsafe.MAX` and `inseguro.MAX` are recognized by a special lexer path rather
than by the regular keyword table.

Some runtime helper aliases, such as `input`/`entrada`, `make_struct`/
`nuevo_estructura`, `push`/`empujar`, `pop`/`sacar`, and `set`/`asignar`, are
handled as identifiers by the runtime rather than as lexer keywords.

## Current Limitations

- The parser recognizes more syntax than the runtime fully implements.
- `main` must not take parameters.
- Structs, arrays, strings, and pointer behavior are still bootstrap-level
  features and may not be stable in every case.
- Modules, packages, classes, inheritance, exceptions, JIT, AOT compilation,
  and garbage collection are not complete runtime features yet.
