[![C](https://img.shields.io/badge/c-2011-gray.svg)](https://en.cppreference.com/w/c)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

# Trazo

Trazo is a small programming language runtime and bootstrap interpreter written in C. It is designed as a bilingual English/Spanish language runtime with a focus on low-level bootstrap execution inside `unsafe.MAX { ... }` blocks.

## What Trazo can do today

- Build with `make` on Unix-like environments and MSYS2/MinGW.
- Parse `.trz` source files and run a simple bootstrap interpreter.
- Print lexer tokens with `--tokens`.
- Execute code inside `unsafe.MAX { ... }` during bootstrap.
- Support basic values: integers, floats, booleans, strings, and `null`.
- Support arithmetic, comparison, logical, and bitwise operators.
- Execute control flow constructs: `if` / `elif` / `else`, `while`, `for`, `break`, `continue`.
- Provide builtin helpers such as `print`, file I/O, memory helpers, and simple array operations.
- Include `radar/` as a related parser toolchain and support area.

## What Trazo can't do yet

- Full module or package import semantics.
- A complete runtime for user-defined functions, classes, and object inheritance.
- Fully stable struct, array, and string behavior in all cases.
- A production-ready standard library or runtime garbage collection.
- Transparent JIT execution in the current bootstrap interpreter.
- Comprehensive exception handling and error recovery.
- Compiling AOT (for now it's only interpreted).
- A Garbage Collector.

## Build and run

From the repository root:

```bash
make
```

On Windows with MSYS2/MinGW, the same `make` command should build `bin/Trazo.exe`.

Run a source file:

```bash
./bin/Trazo.exe examples_ejemplos/hola_mundo.trz
```

Show lexer tokens only:

```bash
./bin/Trazo.exe --tokens examples_ejemplos/hola_mundo.trz
```

Show help:

```bash
./bin/Trazo.exe --help
```

## Repository layout

- `core/src/` — core compiler/runtime sources for Trazo.
- `core/decNumber/` — decimal arithmetic library used by the runtime.
- `examples_ejemplos/` — example Trazo source files.
- `radar/` — parser/toolchain subproject and support libraries.
- `bin/` — build output directory.
- `build/` — object files and intermediate build artifacts.

## Notes

The current execution engine is focused on bootstrap code inside `unsafe.MAX { ... }`. Many parser features are planned or partially implemented, but runtime support remains limited at this stage.

## License

This project is licensed under the Apache License, Version 2.0. See `LICENSE` for details.
