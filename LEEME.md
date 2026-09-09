[![C](https://img.shields.io/badge/c-2011-gray.svg)](https://en.cppreference.com/w/c)
[![Licence](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

# Trazo

Trazo es un lenguaje de programación y un compilador experimentales. Los archivos
fuente de Trazo utilizan la extensión `.trz` y se traducen a C antes de ser
compilados por GCC, Clang o MSVC.

El proyecto se encuentra actualmente en fase alfa. El compilador está
implementado en C y su objetivo a largo plazo es lograr el *bootstrap* (autocompilación)
de Trazo reescribiendo el compilador en el propio lenguaje Trazo.

## Qué incluye

- Sintaxis de Trazo bilingüe (inglés/español).
- Funciones, tipos básicos, estructuras (`structs`), uniones, enumeraciones (`enums`),
flujo de control, punteros y código inseguro (*unsafe code*).
- Interoperabilidad con C mediante `importC` y llamadas del tipo `C.name(...)`.
- Módulos, importaciones, generación de código C y archivos de cabecera,
verificación de tipos y *name mangling* (codificación de nombres de símbolos).
- Radar, una pequeña herramienta de construcción declarativa basada en TOML.

## Requisitos

- Un compilador de C: GCC, Clang o MSVC.
- Una biblioteca estándar compatible con C11.
- Un intérprete de comandos (*shell*) adecuado para ejecutar los comandos indicados más abajo.

## Construir Trazo con Radar

El manifiesto del proyecto Radar es [Radar/Radar.toml](Radar/Radar.toml). El proceso
actual de construcción inicial (*bootstrap*) compila Radar directamente y, a
continuación, utiliza Radar para compilar la implementación en C de Trazo. Desde la raíz del repositorio:

```text
gcc -std=c11 -O2 -Wall -Wextra -Werror -Wshadow -Wvla \
-Wconversion -Wformat-security -Werror=format \
-fstack-protector-strong -D_FORTIFY_SOURCE=2 \
Radar/lexer.c Radar/parser.c Radar/validator.c \
Radar/build_plan.c Radar/main.c -o Radar/radar

./Radar/radar check Radar/Radar.toml
./Radar/radar plan Radar/Radar.toml
./Radar/radar build Radar/Radar.toml
```

En Windows con PowerShell, utiliza `Radar\\radar.exe` en lugar de
`./Radar/radar`.

El compilador resultante se genera en `bin/Trazo` o `bin/Trazo.exe`.

## Uso de Trazo

Generar código C a partir de un programa Trazo:

```text
bin/Trazo --cOnly --output=bin/hello-generated.c \
examples_ejemplos/hello_world.trz
```

Compilar un programa Trazo como ejecutable:

```text
bin/Trazo examples_ejemplos/hello_world.trz
```

Inspeccionar los tokens del analizador léxico de Trazo con:

```text
bin/Trazo --tokens examples_ejemplos/hello_world.trz
```

Inspeccionar los tokens del analizador léxico de Radar con:

```text
./Radar/radar --tokens Radar/Radar.toml
```

## Radar

Radar utiliza datos TOML estándar y no incorpora lógica de compilación. Sus campos principales
se definen bajo `[build]`:

```toml
[build]
kind = "program"
compiler = "auto"
output = "../bin/Trazo"
include = ["../Trazocore/src"]
std-c = "c11"
sources = ["../Trazocore/src/main.c"]
```

El campo `compiler` puede ser `"auto"`, `"gcc"`, `"clang"` o `"msvc"`; si se omite,
se asume `"auto"`. Actualmente, los archivos fuente deben listarse explícitamente. El MVP actual de Radar
admite la validación, la inspección de planes, la inspección de tokens y la compilación de programas.

Hay más detalles disponibles en [Radar/README.md](Radar/README.md).

## Estructura del proyecto

- `Trazocore/src/`: Implementación en C del compilador Trazo.
- `Radar/`: Analizador sintáctico de TOML y herramienta de compilación.
- `examples_ejemplos/`: Ejemplos de Trazo.
- `tests/`: Archivos de prueba (fixtures) de código fuente de Trazo.
- `docs/`: Documentación de la ABI y del proyecto.
- `.github/workflows/ci.yml`: CI nativa para Linux, Windows y macOS.

## Limitaciones actuales

- Trazo es un software en fase alfa; tanto el lenguaje como la ABI pueden sufrir cambios.
- Radar aún no evalúa scripts de compilación ni descubre fuentes de forma dinámica.
- El compilador de arranque (*bootstrap compiler*) sigue implementado en C.
- El código C generado y el comportamiento del *backend* aún no constituyen una garantía de compatibilidad estable.

## Licencia

Trazo se distribuye bajo la Licencia Apache 2.0 con la excepción de LLVM.
Consulte [LICENSE](LICENSE) y [NOTICE](NOTICE).