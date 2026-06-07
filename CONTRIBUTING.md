# Contributing to Trazo

Thank you for your interest in contributing to Trazo. This project is under active development, and contributions that improve clarity, stability, or documentation are welcome.

## How to contribute

1. Fork the repository and create a branch for your work.
2. Build the project locally with `make`.
3. Verify your changes in the code or documentation.
4. Open a pull request with a clear description of what changed and why.

## Recommended workflow

- Use feature branches: `feature/<name>` or `fix/<description>`.
- Keep commits small and focused.
- Explain design decisions in the PR description.
- Update both English and Spanish docs when behavior or syntax changes.

## Project conventions

- Keep C source code simple and readable.
- Avoid adding platform-specific build behavior unless necessary.
- New language features should be documented in `SYNTAX.md` and `SYNTAX.es.md`.
- Example files belong in `examples_ejemplos/`.

## What to work on first

Suggested improvements include:

- Stabilizing runtime support for bootstrap features.
- Making `unsafe.MAX` execution reliable and consistent.
- Fixing parser memory management and AST handling.
- Adding clear examples for current language capabilities.

## What not to change yet

- Do not modify vendor or third-party code unless necessary for build support.

## License and conduct

By contributing, you agree that your contributions will be licensed under the Apache License 2.0 and that you will follow the project Code of Conduct.
