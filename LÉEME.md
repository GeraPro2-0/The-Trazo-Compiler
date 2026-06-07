[![C](https://img.shields.io/badge/c-2011-blue.svg)](https://en.cppreference.com/w/c)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

# Trazo

Trazo es un intérprete de lenguaje de programación pequeño y bilingüe, implementado en C. El motor actual ejecuta código de arranque contenido en bloques `unsafe.MAX { ... }`.

## Qué puede hacer Trazo hoy

- Compilar con `make` en entornos Unix-like y MSYS2/MinGW.
- Analizar archivos `.trz` y ejecutar un intérprete de arranque simple.
- Mostrar tokens del lexer con `--tokens`.
- Ejecutar código dentro de `unsafe.MAX { ... }` durante el bootstrap.
- Soportar valores básicos: enteros, flotantes, booleanos, cadenas y `null`.
- Soportar operadores aritméticos, de comparación, lógicos y bitwise.
- Ejecutar control de flujo: `if` / `elif` / `else`, `while`, `for`, `break`, `continue`.
- Proveer helpers integrados como `print`, operaciones de archivos, memoria y arrays simples.
- Incluir `radar/` como una herramienta de parser y área de soporte.

## Qué no puede hacer todavía

- Un sistema completo de módulos o imports.
- Un runtime completo para funciones definidas por usuario, clases y herencia.
- Comportamiento estable de structs, arrays y strings en todos los casos.
- Una biblioteca estándar de producción o recolección de basura robusta.
- Ejecución JIT integrada en el intérprete de bootstrap actual.
- Manejo de excepciones y recuperación de errores avanzada.
- Compilar a AOT (por ahora solo es interpretado).
- Un Recolector de Basura (Garbage Collector).

## Construcción y ejecución

Desde la raíz del repositorio:

```bash
make
```

En Windows con MSYS2/MinGW, el mismo comando `make` genera `bin/Trazo.exe`.

Ejecuta un archivo de código fuente:

```bash
./bin/Trazo.exe examples_ejemplos/hola_mundo.trz
```

Mostrar tokens del lexer:

```bash
./bin/Trazo.exe --tokens examples_ejemplos/hola_mundo.trz
```

Mostrar ayuda:

```bash
./bin/Trazo.exe --help
```

## Estructura del repositorio

- `core/src/` — fuentes del compilador/runtime principal de Trazo.
- `core/decNumber/` — biblioteca de aritmética decimal usada por el runtime.
- `examples_ejemplos/` — ejemplos de código Trazo.
- `radar/` — subproyecto de parser y herramientas de soporte.
- `bin/` — ejecutables generados.
- `build/` — artefactos intermedios de compilación.

## Notas

El motor actual está centrado en la ejecución de bootstrap dentro de `unsafe.MAX { ... }`. Muchas características del parser están planeadas o parcialmente implementadas, pero el soporte de runtime sigue siendo limitado en esta etapa.

## Licencia

Este proyecto está licenciado bajo Apache License 2.0. Ver `LICENSE` para más detalles.
