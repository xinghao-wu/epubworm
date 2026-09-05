#pragma once

#include <filesystem>
#include <string_view>

// Validates and dispatches command-line arguments.
// Returns zero on success and nonzero for invalid arguments or an operation
// that could not be completed.
int dispatchCli(int argc, char** argv);

// Helper for `displayHelp()`
void printAligned(std::string_view left, std::string_view right);

// Prints the program's help text to stdout.
// Section headers are styled bold + colored when stdout is a terminal.
void displayHelp();

// Lists the title, author, and ID prefix of every epub in the library at
// `shareAbs/epubworm`, sorted by title. Each ID is shown as an unambiguous
// prefix of at least four characters. Fields are styled when stdout is a
// terminal. Title and author are read from each extracted epub's OPF at
// `shareAbs/epubworm/extracted_epubs/<id>/`. Prints the last-read epub's title
// after the entries, or a message if it is no longer in the library. Prints a
// message indicating the library is empty if there are no `<epub>` entries.
// Status messages are styled when stdout is a terminal. Throws
// `std::runtime_error` on library load failure, missing
// `<library>` or `<last-read>`, an `<epub>` or `<last-read>` missing its `id`
// attribute, or failure to load/parse an extracted epub's OPF.
void listLibrary(const std::filesystem::path& shareAbs);
