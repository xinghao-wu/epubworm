# Agent Guidelines

## Project Context
term-epub-img is an image-capable TUI epub reader written in C++23.

## Command Reference
```bash
make fmt        # 1. Format code with clang-format.
make lint       # 2. Run clang-tidy (ignore "suppressed warnings" from this).
make test       # 3. Build and run the test suite.
make            # 4. Release build; produces `build/tei`.
```

## Repository Structure
- Project source and vendored libraries live flat in `src/`.
- Unzipped and zipped epubs used for testing are found at `test_epubs/`.

## Vendored Libraries
- tinyxml2
- miniz_cpp
- stb_image
- base64

## Non-Self-Validating Tests
To run a non-self-validating test, add a call to it in `src/test_main.cpp`.
