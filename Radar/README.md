# Radar

Radar is Trazo's TOML-based build tool. Its project manifest is named
`Radar.toml`.

## Module layout

- `radar.h`: public token and parser interfaces.
- `lexer.c`: TOML and Radar tokenization.
- `parser.c`: TOML syntax validation.
- `validator.c`: semantic validation for TOML scalar literals.
- `build_plan.c`: compiler-neutral extraction of the `[build]` configuration.
- `main.c`: `radar` command-line entry point, including the `--tokens` diagnostic mode.
- `Makefile`: local Radar tool build only; it does not build Trazo itself yet.

The manifest is TOML and contains no executable logic:

```toml
[project]
name = "Trazo"
version = "0.1.0"

[build]
kind = "program"
compiler = "auto"
sources = ["src/main.trz"]
```

## Design rules

- TOML tables, arrays, strings, booleans, numbers and dates retain TOML semantics.
- Paths are resolved relative to `Radar.toml`, not the current shell directory.
- Build logic belongs in Trazo programs or future tooling, not in the manifest.
- The manifest is deterministic data and has no side effects.

## TOML foundation

The parser supports the core TOML data model without evaluating build logic:

- key-value pairs and dotted keys;
- `[table]` and nested `[table.path]` headers;
- `[[array.of.tables]]` headers;
- basic and multiline strings, literal and multiline literal strings;
- booleans, integers, floats, and date/time values;
- decimal numbers with separators plus hexadecimal, octal, and binary integers;
- arrays, including trailing commas;
- inline tables such as `{ optimization = "debug", warnings = true }`.
- valid basic-string escapes, including `\n`, `\uXXXX`, and `\UXXXXXXXX`;
- numeric separators only between digits and valid prefixed integer forms.

[toml-base.radar](toml-base.radar) and [toml-scalars.radar](toml-scalars.radar)
are the parser fixtures for this phase. [toml-invalid.radar](toml-invalid.radar)
and [toml-invalid-date.radar](toml-invalid-date.radar) verify that malformed
escapes and out-of-range calendar values are rejected.

## BuildPlan MVP

Use `radar plan Radar.toml` to inspect the compiler-neutral build intent. The
current plan extracts static `[build]` values such as `compiler`, `kind`,
`output`, `std-c`, `include`, `sources`, and `flags`.

The configuration can also use the documented Spanish table and key aliases:

```toml
[proyecto]
nombre = "Trazo"
version = "0.1.0"

[construccion]
tipo = "programa"
compilador = "auto"
salida = "../bin/Trazo"
incluir = ["../core/src"]
fuentes = []
banderas = ["-Og", "-g"]
```

The initial key aliases are:

| English | Spanish |
| --- | --- |
| `project` | `proyecto` |
| `build` | `construccion` |
| `name` | `nombre` |
| `kind` | `tipo` |
| `compiler` | `compilador` |
| `output` | `salida` |
| `include` | `incluir` |
| `sources` | `fuentes` |
| `flags` | `banderas` |

The manifest may use English or Spanish aliases, but remains TOML data.

## MVP commands

The first usable command is validation:

```text
radar check Radar.toml
```

It runs lexing, TOML scalar validation, and syntax parsing in that order. The
direct form `radar Radar.toml` remains accepted.

The MVP build command consumes the static `[build]` plan and invokes the
selected C compiler:

```text
radar build Radar.toml
```

It supports `program` builds, `compiler = "auto"`, `"gcc"`, `"clang"`, or
`"msvc"` (omitting `compiler` also means `"auto"`), `std-c`, include paths,
source arrays, flags, and output paths. Paths are resolved relative to the
`Radar.toml` file. Sources must be listed explicitly; there is no evaluator.

`radar plan Radar.toml` prints the same compiler-neutral values without
invoking the compiler. `clean` is the next MVP slice.

Radar remains a manifest parser and build orchestrator. Trazo owns compiler
selection and backend-specific behavior.