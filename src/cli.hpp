#pragma once

#include <filesystem>
#include <string_view>

// Validates and dispatches command-line arguments.
// Returns zero on success and nonzero for invalid arguments or an operation
// that could not be completed.
int dispatchCli(int argc, char** argv);

// Helper for `displayHelp()`
void printAligned(std::string_view left, std::string_view right);

// Prints the program's help text (usage, options, commands, TUI keybinds) to
// stdout.
// Section headers are styled bold + colored when stdout is a terminal;
// command names and descriptions remain plain text.
void displayHelp();

// Lists every epub in the library at `shareAbs/tei`, sorted by title and
// printing each as `title - author - id` to stdout. Fields are styled when
// stdout is a terminal. Title and author are read from each extracted epub's
// OPF at `shareAbs/tei/extracted_epubs/<id>/`.
// Prints a message indicating the library is empty if there are no `<epub>`
// entries.
// Throws `std::runtime_error` on library load failure, missing
// `<library>` root, an `<epub>` missing its `id` attribute, or failure to
// load/parse an extracted epub's OPF.
void listLibrary(const std::filesystem::path& shareAbs);
