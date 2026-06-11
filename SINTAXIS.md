# Sintaxis de Trazo

Este archivo describe la sintaxis soportada por la implementación actual de
Trazo. El lexer acepta palabras clave en inglés y en español para el mismo
token, así que ambos estilos pueden mezclarse, aunque los ejemplos normalmente
mantienen un solo estilo por archivo.

## Comentarios

Trazo soporta:

- `//` para comentarios de una línea.
- `/* ... */` para comentarios de bloque.

## Espacio en Blanco

El espacio en blanco y los saltos de línea separan tokens y se ignoran en el
resto. Las sentencias normalmente terminan con `;`.

## Identificadores

Los identificadores comienzan con una letra o guion bajo y pueden contener
letras, dígitos y guiones bajos:

```trazo
mi_valor
_nombre
ptr123
```

## Literales

- Literales enteros: `42`, `0x10`, `0b1010`
- Literales flotantes: `3.14`, `0.5`, `1.0e3`, `2.0f`
- Booleanos: `true`, `false`
- Cadenas: `"hola"`
- Caracteres: `'a'`
- Nulo: `null`

También se aceptan los literales en español: `verdadero`, `falso` y `nulo`.

## Tipos y Declaraciones

Las declaraciones de variables usan un tipo explícito:

```trazo
entero x = 10;
cadena saludo = "hola";
entero* ptr = reservar(16);
```

Los tipos incorporados soportados incluyen:

- `int`, `i8`, `i16`, `i32`, `i64`
- `u8`, `u16`, `u32`, `u64`, `byte`
- `char`, `rune`
- `f32`, `f64`, `float`, `double`
- `bool`, `string`, `void`
- `decimal`, `dec32`, `dec64`, `dec128`
- `ptr`

Los tipos de puntero se escriben con `*`, como `entero*` o `flotante*`.
Las declaraciones también pueden usar el prefijo `volatil`.

## Expresiones

Trazo soporta formas comunes de expresión:

- Asignación: `nombre = valor`
- Aritmética binaria: `+`, `-`, `*`, `/`, `%`
- Comparación: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Lógicos: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Unarios: `-`, `!`, `~`, `&`, `*`
- Paréntesis: `(expr)`
- Conversiones: `expr como tipo`
- Acceso a campos: `valor.campo`
- Acceso a campos por puntero: `valor->campo`
- Indexación: `array[indice]`
- Llamadas a funciones: `fn(arg1, arg2)`

Las operaciones de dirección y desreferencia de punteros requieren un contexto
inseguro.

## Control de Flujo

### Sentencia If

```trazo
si (x > 0) {
    imprimir("positivo");
} sino_si (x == 0) {
    imprimir("cero");
} sino {
    imprimir("negativo");
}
```

### Bucle While

```trazo
entero i = 0;
mientras (i < 3) {
    imprimir(i);
    i = i + 1;
}
```

### Bucle For

```trazo
para (entero i = 0; i < 3; i = i + 1) {
    imprimir(i);
}
```

### Break y Continue

```trazo
para (entero i = 0; i < 5; i = i + 1) {
    si (i == 2) {
        continuar;
    }
    si (i == 4) {
        romper;
    }
    imprimir(i);
}
```

## Funciones

Las funciones se declaran con `funcion`. El tipo de retorno puede declararse
con `->`.

```trazo
funcion saludar(cadena nombre) {
    imprimir("Hola", nombre);
}

funcion main -> entero {
    saludar("Trazo");
    retornar 0;
}
```

El runtime busca una función `main` sin parámetros y la ejecuta cuando existe.

## Código Inseguro

Las operaciones inseguras deben ejecutarse dentro de `unsafe.MAX` o `inseguro.MAX`.

Un bloque inseguro:

```trazo
funcion main -> entero {
    inseguro.MAX {
        entero value = 42;
        entero* ptr = &value;
        *ptr = 99;
    }
    retornar 0;
}
```

Una función insegura:

```trazo
unsafe.MAX funcion main -> entero {
    entero* buffer = reservar(64);
    llenmem(buffer, 0, 64);
    liberar(buffer);
    retornar 0;
}
```

También se reconoce la forma en inglés `unsafe.MAX`.

## Estructuras

Las estructuras se declaran con `estructura`. Los valores de estructura en
runtime pueden crearse con `nuevo_estructura()` y luego recibir campos.

```trazo
estructura Perfil {
    entero edad;
    cadena nombre;
}

funcion main -> entero {
    Perfil persona = nuevo_estructura();
    persona.edad = 31;
    persona.nombre = "Rosa";
    imprimir(persona.nombre, persona.edad);
    retornar 0;
}
```

## Arrays

Los literales de array usan corchetes:

```trazo
funcion main -> entero {
    entero numeros = [1, 2, 3];
    imprimir(longitud(numeros));
    empujar(numeros, 4);
    imprimir(numeros[1]);
    retornar 0;
}
```

Los helpers actuales incluyen `empujar(array, valor)`, `sacar(array)`,
`asignar(array, indice, valor)` y `longitud(array)`.

## Declaraciones Extern

Las declaraciones externas pueden exponer a Trazo un conjunto pequeño de
llamadas soportadas de la biblioteca de C.

```trazo
externo entero puts(cadena texto);
externo entero strlen(cadena texto);

funcion main -> entero {
    puts("llamado desde Trazo");
    imprimir(strlen("Trazo"));
    retornar 0;
}
```

El runtime actual tiene manejo especial para funciones como `puts`, `putchar`,
`strlen`, `strcmp`, `getenv`, `atoi` y `exit`.

## Helpers Incorporados

Algunos helpers comunes del runtime son:

- `imprimir(...)`
- `entrada(prompt?)`
- `leer_linea()`
- `leer_archivo(ruta)`
- `escribir_archivo(ruta, datos)`
- `anexar_archivo(ruta, datos)`
- `archivo_existe(ruta)`
- `longitud(valor)`
- `nuevo_estructura()`
- `empujar(array, valor)`
- `sacar(array)`
- `asignar(array, indice, valor)`
- `tamano_de(tipo_o_valor)`
- `reservar(tamano)`, `asignar_mem(tamano)`, `asignar_cero(cantidad, tamano)`, `reasignar(ptr, tamano)`
- `liberar(ptr)`, `eliminar(ptr)`
- `copimem(dst, src, tamano)`, `llenmem(dst, byte, tamano)`, `comparmem(a, b, tamano)`

La memoria manual, las operaciones de memoria y los punteros requieren
`unsafe.MAX`.

## Tabla de Palabras Clave del Lexer

Estas son las parejas de palabras clave en inglés y español listadas
actualmente en `core/src/lexer.c`.

|    Inglés     |      Español       |
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

`unsafe.MAX` e `inseguro.MAX` se reconocen mediante una ruta especial del
lexer, no mediante la tabla regular de palabras clave.

Algunos alias de helpers del runtime, como `input`/`entrada`,
`make_struct`/`nuevo_estructura`, `push`/`empujar`, `pop`/`sacar` y
`set`/`asignar`, se manejan como identificadores por el runtime, no como
palabras clave del lexer.

## Limitaciones Actuales

- El parser reconoce más sintaxis de la que el runtime implementa por completo.
- `main` no debe recibir parámetros.
- Las estructuras, arrays, cadenas y punteros siguen siendo características de
  bootstrap y pueden no ser estables en todos los casos.
- Módulos, paquetes, clases, herencia, excepciones, JIT, compilación AOT y
  recolección de basura todavía no son características completas del runtime.
