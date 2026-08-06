# AGENTS.md
Monocle (mnc) is a TUI epub reader written in C++23.

## Commands
- Build: `make` in repo root produces `build/mnc`.
- Format: `diff -u <file> <(clang-format <file>)` prints diff of original vs
  formatted file, `clang-format -i <file>` edits the file in-place.
- Lint: `clang-tidy <file>` prints linter diagnostics

## Project Structure
- Project source and vendored libraries live flat in `src/`.
- Unzipped and zipped epubs used for testing are found in `test_epubs/`.
- `.testing_xdg_dirs/` contains phony XDG directories used for testing.

## Vendored Libraries
- `tinyxml2.*`: XML handling.
- `miniz_cpp.hpp`: Zip handling.
- `stb_image.hpp`: Image data parsing.
- `base64.hpp`: Base64 handling.
Vendored headers contain pragmas to be treated as system headers, and vendored
source files are compiled with `-w`. Tools will not produce warnings for these
files.

## Project Source
- `data_management.*`: Persistent data handling (config, library, etc.).
- `epub_parser.*`: Parses data from unzipped epubs.
- `tui.*`: Prepares parsed epub data for display and draws the TUI.
- `test.*`: Where all tests live in the `test` namespace.
- `main.cpp`: Stub entry point.

Helpers factored out into their own files because of length:
- `percent_encoding_decode.hpp`
- `row_col_diacritics.hpp`

## Testing
To run a test, add a call to it in `main()`, rebuild, and run the binary. Tests
depend on the executable being ran with `build/` as the CWD.
