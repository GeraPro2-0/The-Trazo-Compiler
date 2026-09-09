[![C](https://img.shields.io/badge/c-2011-gray.svg)](https://en.cppreference.com/w/c)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

# Trazo

Trazo is an experimental programming language and compiler. Trazo source files
use the `.trz` extension and are translated to C before being compiled by GCC,
Clang, or MSVC.

The project is currently alpha software. The compiler is implemented in C and
its long-term goal is to bootstrap Trazo by rewriting the compiler in Trazo.

## What is included

- A bilingual English/Spanish Trazo syntax.
- Functions, basic types, structs, unions, enums, control flow, pointers, and
  unsafe code.
- C interoperability through `importC` and `C.name(...)` calls.
- Modules, imports, generated C and headers, type checking, and symbol mangling.
- Radar, a small declarative TOML-based build tool.

See the complete bilingual language reference in
[docs/syntax.md](docs/syntax.md).

## Requirements

- A C compiler: GCC, Clang, or MSVC.
- A C11-compatible standard library.
- A shell suitable for the commands below.

## Build Trazo with Radar

Radar's project manifest is [Radar/Radar.toml](Radar/Radar.toml). The current
bootstrap build compiles Radar directly, then uses Radar to compile the C
implementation of Trazo.

From the repository root:

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

On Windows with PowerShell, use `Radar\\radar.exe` instead of
`./Radar/radar`.

The resulting compiler is written to `bin/Trazo` or `bin/Trazo.exe`.

## Use Trazo

Generate C output from a Trazo program:

```text
bin/Trazo --cOnly --output=bin/hello-generated.c \
  examples_ejemplos/hello_world.trz
```

Build a Trazo program as an executable:

```text
bin/Trazo examples_ejemplos/hello_world.trz
```

Inspect Trazo lexer tokens with:

```text
bin/Trazo --tokens examples_ejemplos/hello_world.trz
```

Inspect Radar lexer tokens with:

```text
./Radar/radar --tokens Radar/Radar.toml
```

## Radar

Radar uses ordinary TOML data and does not embed build logic. Its main fields
are defined under `[build]`:

```toml
[build]
kind = "program"
compiler = "auto"
output = "../bin/Trazo"
include = ["../Trazocore/src"]
std-c = "c11"
sources = ["../Trazocore/src/main.c"]
```

`compiler` may be `"auto"`, `"gcc"`, `"clang"`, or `"msvc"`; omitting it
means `"auto"`. Sources must currently be listed explicitly. Radar's current
MVP supports validation, plan inspection, token inspection, and program builds.

More details are available in [Radar/README.md](Radar/README.md).

## Project layout

- `Trazocore/src/`: C implementation of the Trazo compiler.
- `Radar/`: TOML parser and build tool.
- `examples_ejemplos/`: Trazo examples.
- `tests/`: Trazo source fixtures.
- `docs/`: ABI and project documentation.
- `.github/workflows/ci.yml`: native CI for Linux, Windows, and macOS.

## Current limitations

- Trazo is alpha software and its language and ABI may change.
- Radar does not yet evaluate build scripts or discover sources dynamically.
- The bootstrap compiler is still implemented in C.
- Generated C output and backend behavior are not yet a stable compatibility
  guarantee.

## License

Trazo is distributed under the Apache License 2.0 with the LLVM Exception.
See [LICENSE](LICENSE) and [NOTICE](NOTICE).
