# AGENTS.md
Monocle (mnc) is a TUI epub reader written in C++23.

## Command Reference
```bash
make            # debug build
make fmt-diff   # show clang-format formatting proposal
make fmt        # make suggested formatting edits in-place
make lint       # run clang-tidy
make lint-fix   # apply safe clang-tidy fixes
```

## Project Structure
- Project source and vendored libraries live flat in `src/`.
- Unzipped and zipped epubs used for testing are found in `test_epubs/`.
- `.testing_xdg_dirs/` contains phony XDG directories used for testing.
- `build/` houses object files and final binary from the build process.

## Notable Files
- `.clang-format`: LLVM style with minor modifications
- `.clang-tidy`: all diagnostics enabled baseline with many exclusions
- `compile_flags.txt`: compilation flags referenced by clangd and clang-tidy
- `build/mnc`: built executable

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
- `main.cpp`: stub entry point

Helpers factored out into their own files because of length:
- `percent_encoding_decode.hpp`
- `row_col_diacritics.hpp`

## Testing
To run a test, add a call to it in `main()`, rebuild, and run the binary. Tests
depend on the executable being ran with `build/` as the CWD.
