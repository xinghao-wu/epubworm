# AGENTS.md
Monocle (mnc) is a TUI epub reader written in C++23.

## Command Reference
```bash
# release build (default); produces `build/mnc`
make

# debug build; produces `build/mnc_debug`
make debug

# Build and run the test suite.
# Produces `build/mnc_test` and runs it with `build/` as the CWD.
make test

# Runs clang-format read-only to check for incorrect formatting.
make fmt-check

# Show clang-format's proposed formatting edits.
make fmt-check-diff

# Make clang-format's proposed formatting edits in-place.
make fmt

# run clang-tidy
make lint

# apply safe clang-tidy fixes
make lint-fix
```

## Project Structure
- Project source and vendored libraries live flat in `src/`.
- Unzipped and zipped epubs used for testing are found in `test_epubs/`.
- `.testing_xdg_dirs/` contains phony XDG directories used for testing.
- `build/` houses built binaries and object files.

## Notable Files
- `.clang-format`: LLVM style with minor modifications
- `.clang-tidy`: all diagnostics enabled baseline with many exclusions
- `compile_flags.txt`: compilation flags referenced by clangd and clang-tidy

## Vendored Libraries
- `tinyxml2.*`: XML handling
- `miniz_cpp.hpp`: zip handling
- `stb_image.hpp`: image data parsing
- `base64.hpp`: base64 handling
Vendored headers contain pragmas to be treated as system headers, and vendored
source files are compiled with `-w`. Tools will not produce warnings for these
files.

## Project Source
- `data_management.*`: persistent data handling (config, library, etc.)
- `epub_parser.*`: Parses data from unzipped epubs.
- `tui.*`: Prepares parsed epub data for display and draws the TUI.
- `test.*`: where all tests live in the `test` namespace
- `test_main.cpp`: test runner entry point, invokes every self-validating test
- `main.cpp`: stub program entry point

Helpers factored out into their own files because of length:
- `percent_encoding_decode.hpp`
- `row_col_diacritics.hpp`

## Non-Self-Validating Tests
To run a non-self-validating test, add a call to it in `test_main.cpp`'s
`main()` after the self-validating tests and run `make test`.
