# Agent Guidelines

## Project Context
epubworm is an image-capable TUI epub reader written in C++23.

## Command Reference
```bash
make fmt     # Runs clang-format to format code.
make lint    # Lints with clang-tidy (ignore "suppressed warnings" from this).
make test    # Builds and runs the test suite.
make         # Builds release binary.
```

## Repository Structure
- Project source lives in `src/`.
- Vendored libraries live in per-library directories under `vendor/`.
- Unzipped and zipped test fixture epubs are found at `fixtures/epubs/`.

## Vendored Libraries
- tinyxml2
- miniz_cpp
- stb_image
- base64

## Non-Self-Validating Tests
To run a non-self-validating test, add a call to it in `src/test_main.cpp`.
