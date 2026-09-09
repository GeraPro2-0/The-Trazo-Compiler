# Trazo Syntax / Sintaxis de Trazo

Trazo is a C-oriented programming language. Its source files use the `.trz`
extension. The compiler accepts English keywords and Spanish aliases; both forms
may be mixed in the same file.

Trazo generates C and can call C functions through the `C` namespace. See
[abi.md](abi.md) for symbol mangling and linking rules.

## Minimal program / Programa minimo

English:

```trazo
importC <stdio.h>

func main() -> int {
    C.printf("Hello, World!\n");
    return 0;
}
```

Spanish:

```trazo
importarC <stdio.h>

funcion main() -> entero {
    C.printf("Hola, mundo!\n");
    retornar 0;
}
```

`main` is the executable entry point. `importC` and `importarC` include a C
header so functions such as `C.printf` can be called.

## Keywords / Palabras clave

| English | Spanish | Meaning / Uso |
| --- | --- | --- |
| `break` | `romper` | Leave a loop or switch / salir de un bucle o `switch` |
| `case` | `caso` | A switch branch / rama de `switch` |
| `char` | `caracter` | Character type / tipo caracter |
| `const` | `constante` | Read-only declaration / declaracion de solo lectura |
| `continue` | `continuar` | Skip to the next loop iteration / siguiente iteracion |
| `default` | `pordefecto` | Fallback switch branch / rama alternativa |
| `else` | `sino` | Alternative conditional branch / rama alternativa |
| `enum` | `enumeracion` | Enumeration declaration / declaracion de enumeracion |
| `extern` | `externo` | External linkage declaration / enlace externo |
| `float` | `flotante` | Floating-point type / tipo decimal |
| `for` | `para` | Counting loop / bucle de conteo |
| `from` | `desde` | Import source / origen de importacion |
| `func` | `funcion` | Function declaration / declaracion de funcion |
| `if` | `si` | Conditional / condicion |
| `import` | `importar` | Trazo module import / importacion de modulo Trazo |
| `importC` | `importarC` | C header import / importacion de header C |
| `int` | `entero` | Integer type / tipo entero |
| `return` | `retornar` | Return from a function / devolver un valor |
| `static` | `estatico` | Static declaration / declaracion estatica |
| `struct` | `estructura` | Structure declaration / declaracion de estructura |
| `switch` | `cambiar` | Multi-branch selection / seleccion de ramas |
| `typedef` | `definirtipo` | Type alias / alias de tipo |
| `union` | `union` | Union declaration / declaracion union |
| `void` | `vacio` | No-value type / tipo sin valor |
| `volatile` | `volatil` | Volatile declaration / declaracion volatil |
| `while` | `mientras` | Conditional loop / bucle condicional |
| `unsafe` | `inseguro` | Unsafe block / bloque no seguro |
| `export` | `exportar` | Public module declaration / declaracion publica |
| `as` | `como` | Import alias / alias de importacion |

## Variables and types / Variables y tipos

Declarations use a type followed by a name. Initialization is optional.

```trazo
int count = 3;
float ratio = 1.5f;
char letter = 'A';
const int limit = 10;
void *raw = 0;
```

The same declarations can use Spanish type names:

```trazo
entero contador = 3;
flotante proporcion = 1.5f;
caracter letra = 'A';
constante entero limite = 10;
```

Supported basic types include `void`/`vacio`, `int`/`entero`, `float`/`flotante`
and `char`/`caracter`. C integer literals, floating-point literals, strings and
character literals are supported according to the current compiler rules.

## Functions / Funciones

A function has a name, parameters, and a return type after `->`.

```trazo
func add(int left, int right) -> int {
    return left + right;
}

funcion sumar(entero izquierdo, entero derecho) -> entero {
    retornar izquierdo + derecho;
}
```

A function without parameters uses an empty parameter list:

```trazo
func ping() -> void {
    return;
}
```

Functions may be overloaded by parameter types:

```trazo
func sum(int left, int right) -> int {
    return left + right;
}

func sum(float left, float right) -> float {
    return left + right;
}
```

The return type is not part of the generated symbol name. See [abi.md](abi.md).

## Conditions / Condiciones

```trazo
if (count > 0) {
    C.printf("positive\n");
} else {
    C.printf("zero or negative\n");
}
```

Spanish aliases:

```trazo
si (contador > 0) {
    C.printf("positivo\n");
} sino {
    C.printf("cero o negativo\n");
}
```

The condition is an expression. Comparisons include `==`, `!=`, `<`, `<=`, `>`
and `>=`; logical operators include `&&` and `||`.

## Loops / Bucles

A `for` loop has initialization, condition, and increment expressions:

```trazo
for (int index = 0; index < 3; index++) {
    C.printf("%d\n", index);
}
```

```trazo
para (entero indice = 0; indice < 3; indice++) {
    C.printf("%d\n", indice);
}
```

A `while` loop runs while its condition is true:

```trazo
while (count < 3) {
    count++;
}

mientras (contador < 3) {
    contador++;
}
```

`break`/`romper` exits the current loop. `continue`/`continuar` skips to its
next iteration.

## Switch / Seleccion multiple

```trazo
switch (value) {
    case 1:
        C.printf("one\n");
        break;
    case 2:
        C.printf("two\n");
        break;
    default:
        C.printf("other\n");
        break;
}
```

Spanish aliases are `cambiar`, `caso` and `pordefecto`:

```trazo
cambiar (valor) {
    caso 1:
        C.printf("uno\n");
        romper;
    pordefecto:
        C.printf("otro\n");
        romper;
}
```

## Arrays and indexing / Arrays e indices

```trazo
int values[3];
values[0] = 10;
values[1] = 20;
values[2] = 30;
C.printf("%d\n", values[1]);
```

Array indexing starts at zero and uses the C-style `array[index]` form.

## Structs, unions, enums and typedefs

```trazo
typedef int Number;

enum Color {
    RED,
    GREEN,
    BLUE
};

struct Point {
    int x;
    int y;
};

union Value {
    int integer;
    float decimal;
};

Point point;
point.x = 10;
point.y = 20;
```

Spanish forms are available:

```trazo
definirtipo entero Numero;

enumeracion Color {
    ROJO,
    VERDE,
    AZUL
};

estructura Punto {
    entero x;
    entero y;
};

union Valor {
    entero numero;
    flotante decimal;
};
```

A `struct` stores all fields. A `union` reuses the same storage for its fields.
`typedef`/`definirtipo` creates an alternate type name.

## Pointers and casts / Punteros y conversiones

```trazo
func show(int *value) -> int {
    C.printf("%d\n", *value);
    return 0;
}

func main() -> int {
    int value = 42;
    int *pointer = &value;
    show(pointer);
    return 0;
}
```

`&` obtains an address, `*` dereferences a pointer, and `->` declares a return
type or accesses a pointer member according to context.

C-style casts and `sizeof` are supported:

```trazo
importC <stdlib.h>

func main() -> int {
    int *pointer = (int*) C.malloc(sizeof(int));
    *pointer = 73;
    C.free(pointer);
    return 0;
}
```

## C interoperability / Interoperabilidad con C

Import a C header and call its declarations through `C.`:

```trazo
importC <stdio.h>

func main() -> int {
    C.printf("value: %d\n", 42);
    return 0;
}
```

`extern "C"` keeps a function's C name and disables Trazo name mangling:

```trazo
extern "C" func c_entry(int value) -> int {
    return value + 1;
}

externo "C" funcion duplicar(entero valor) -> entero {
    retornar valor + valor;
}
```

This is useful when a C caller must link against a Trazo function with a stable
C symbol. `C.name(...)` calls retain the exact C function name.

## Modules and imports / Modulos e importaciones

Trazo modules use `.trz` files. A module can be imported by name:

```trazo
import math

func main() -> int {
    return add(4, 6);
}
```

The import system compiles imported modules together with the entry file and
reports circular imports. `from` and `as` are available for selective imports
and aliases; the exact exported names must exist in the imported module.

## Exporting declarations / Exportar declaraciones

Use `export` or `exportar` to expose a declaration from a module:

```trazo
export func add(int left, int right) -> int {
    return left + right;
}

export estructura Box {
    entero value;
};
```

Non-exported declarations remain private to the module's generated interface.

## Unsafe blocks / Bloques no seguros

```trazo
unsafe {
    int value = 42;
    C.printf("%d\n", value);
}

inseguro {
    C.printf("bloque inseguro\n");
}
```

Unsafe code is accepted without the same safety guarantees as checked code. Use
it only when direct pointer or C operations are required.

## Preprocessor integration / Integracion con el preprocesador

C preprocessor directives can be included in a Trazo source file when they are
needed by imported C interfaces:

```trazo
#include <stdio.h>
#if defined(TARGET)
#endif

func main() -> int {
    C.printf("preprocessor\n");
    return 0;
}
```

## Compilation examples / Ejemplos de compilacion

Generate C and headers without invoking the final C build:

```text
Trazo --cOnly --output=build/hello.c examples_ejemplos/hello_world.trz
```

Build an executable through the selected C backend:

```text
Trazo examples_ejemplos/hello_world.trz
```

Show lexer tokens:

```text
Trazo --tokens examples_ejemplos/hello_world.trz
```

The repository contains additional examples for arrays, casts, modules,
preprocessor directives, switch statements, typedefs, unions and unsafe code
under `examples_ejemplos/`.
