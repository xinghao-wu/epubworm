#pragma once

#include <string_view>

// Helper for `displayHelp()`
void printAligned(std::string_view left, std::string_view right);

// Prints the program's help text (usage, options, commands) to stdout.
// Section headers are styled bold + colored when stdout is a terminal;
// command names and descriptions remain plain text.
void displayHelp();
