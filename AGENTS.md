# AGENTS.md

Monocle (`mnc`) is a TUI epub reader in C++23. Source lives flat in `src/`; vendored libs (`tinyxml2`, `miniz_cpp.hpp`, `stb_image.hpp`, `base64.hpp`) sit alongside project files.

## Build

- `make` from repo root produces `build/mnc`. `make clean` removes `build/`.
- Toolchain: `g++ -std=c++23 -g3 -fsanitize=address` plus `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Weffc++`. Don't relax these to silence warnings — fix the code.
- `tinyxml2.cpp` is compiled with `-w` (vendored third-party). Do not "fix" its warnings.
- The Makefile makes every object depend on `$(wildcard src/*)`, so any src edit triggers a full rebuild. This is intentional.
- No separate lint/typecheck/test targets. clang-tidy config lives in `.clang-tidy`; `compile_flags.txt` feeds clangd.

## Tests

- There is no test runner. `src/test.cpp` is linked into the **same** `build/mnc` binary, and `src/main.cpp` currently just calls `test::readMncConf()`. To run a different test, edit the call in `main.cpp` and rebuild.
- Tests use bare `assert()` plus `std::cout`; many require manual verification (image rendering, TUI screens, raw-mode input). See the `// also serves to test ...` comments in `src/test.hpp` for what each entry point covers.
- **Run from `build/`**: `./mnc` must be invoked with `build/` as cwd. `test.cpp` derives `projectRootAbs` from `fs::current_path().parent_path()` and reads `test_epubs/` and `.testing_xdg_dirs/` relative to it. Running from elsewhere fails immediately.
- Tests write to `.testing_xdg_dirs/{.cache,.config,share}` rather than real XDG dirs. Pre-unzipped epubs live in `test_epubs/*_unzipped/`; zipped `.epub` siblings are used by `test::unzip()`.

## Architecture

- `epub_parser.*` — OPF/spine/TOC parsing, chapter XHTML rendering. Note (header): malformed XML may null-deref by design; don't add defensive null checks unless the crash isn't acceptable.
- `data_management.*` — zip extraction (`miniz`), `mnc/conf.xml` and `mnc/library.xml` init/read.
- `tui.*` — raw mode, SIGWINCH handling, kitty graphics protocol, text layout/wrapping/centering. Highest-level entry: `displayEpub()`.
- `main.cpp` — currently a test driver, not the real app loop.

## Runtime constraints

- Image display uses the kitty graphics protocol. Inside tmux, requires `set -g allow-passthrough on` in `~/.tmux.conf` (see `wrapForTmuxPassthrough()`).
- App config layout at runtime: `<xdg-config>/mnc/conf.xml` and `<xdg-share>/mnc/library.xml`.

## Style

- `.editorconfig`: 4-space indent, 79 col max, trim trailing whitespace.
- `.clang-tidy` enables almost all checks with a long list of repo-specific exclusions (magic numbers, identifier length, braces-around-statements, anonymous namespaces, etc.). Match existing style rather than re-enabling excluded checks.
