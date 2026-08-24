#include "cli.hpp"
#include "tui.hpp"
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

static constexpr int descCol{29};

void printAligned(std::string_view left, std::string_view right) {
    std::cout << left;
    const int pad{descCol - static_cast<int>(left.size())};
    if (pad > 0) {
        std::cout << std::string(static_cast<std::size_t>(pad), ' ');
    }
    std::cout << right << '\n';
}

void displayHelp() {
    boldColorIfTerm(stdout, greenFG);
    std::cout << "Usage:";
    resetBoldColorIfTerm(stdout);
    std::cout << "  tei [OPTIONS] [COMMAND] [ARGS...]\n";
    printAligned("  tei", "Read the last read epub");
    std::cout << '\n';

    boldColorIfTerm(stdout, blueFG);
    std::cout << "Options:";
    resetBoldColorIfTerm(stdout);
    std::cout << '\n';
    printAligned("  -h, --help", "Display this help message");
    std::cout << '\n';

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "Commands:";
    resetBoldColorIfTerm(stdout);
    std::cout << '\n';
    printAligned("  add <file>...", "Add epub files to library");
    printAligned("  remove, rm, delete <id>", "Remove epub from library");
    printAligned("  list, ls", "List library's epubs' id, title, author");
    printAligned("  read <id>", "Read epub from library");
    std::cout << '\n';

    std::cout
            << "All full ids can be substituted with unambiguous prefixes.\n";
}
