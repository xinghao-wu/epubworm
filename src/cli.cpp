#include "cli.hpp"
#include "data_management.hpp"
#include "epub_parser.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

static constexpr int descCol{34};

enum class CliCommand {
    invalid,
    help,
    none,
    add,
    remove,
    list,
    read,
    setLineLength,
};

static void displayError(std::string_view message) {
    boldColorIfTerm(stderr, redFG);
    std::cerr << "error: " << message << '\n';
    resetBoldColorIfTerm(stderr);
}

static CliCommand parseCommand(int argc, char** argv) {
    if (argc <= 1) {
        return CliCommand::none;
    }

    const std::string_view command{argv[1]};
    if (command == "-h" || command == "--help") {
        if (argc == 2) {
            return CliCommand::help;
        }
        displayError("help cannot be ran with other arguments");
        return CliCommand::invalid;
    }

    if (command == "add") {
        if (argc >= 3) {
            return CliCommand::add;
        }
    } else if (command == "remove" || command == "rm" || command == "delete") {
        if (argc == 3) {
            return CliCommand::remove;
        }
    } else if (command == "list" || command == "ls") {
        if (argc == 2) {
            return CliCommand::list;
        }
    } else if (command == "read") {
        if (argc == 3) {
            return CliCommand::read;
        }
    } else if (command == "set-line-length") {
        if (argc == 3) {
            return CliCommand::setLineLength;
        }
    } else {
        displayError("unknown command, run tei -h for usage");
        return CliCommand::invalid;
    }

    displayError("invalid command argument count, run tei -h for usage");
    return CliCommand::invalid;
}

static fs::path getXdgDir(const char* name, const fs::path& fallback) {
    const char* const value{std::getenv(name)};
    if (value != nullptr && fs::path{value}.is_absolute()) {
        return value;
    }

    const char* const home{std::getenv("HOME")};
    if (home == nullptr || !fs::path{home}.is_absolute()) {
        throw std::runtime_error{
                "HOME must be an absolute path when an XDG directory is not "
                "set to an absolute path"};
    }
    return fs::path{home} / fallback;
}

int dispatchCli(int argc, char** argv) {
    const CliCommand command{parseCommand(argc, argv)};

    switch (command) {
    case CliCommand::invalid:
        return 1;
    case CliCommand::help:
        displayHelp();
        return 0;
    default:
        break;
    }

    const fs::path configAbs{getXdgDir("XDG_CONFIG_HOME", ".config")};
    const fs::path shareAbs{getXdgDir("XDG_DATA_HOME", ".local/share")};
    const fs::path teiConfAbs{configAbs / "tei/conf.xml"};
    const fs::path teiLibraryAbs{shareAbs / "tei/library.xml"};

    if (!fs::exists(teiConfAbs)) {
        initConf(teiConfAbs);
    }
    if (!fs::exists(teiLibraryAbs)) {
        initLibrary(teiLibraryAbs);
    }
    const ConfOpts conf{readTeiConf(teiConfAbs)};

    switch (command) {
    case CliCommand::none: {
        XMLDocument library{};
        if (library.LoadFile(teiLibraryAbs.c_str()) != XML_SUCCESS) {
            throw std::runtime_error{
                    std::string{"error loading library file: "}
                    + XMLDocument::ErrorIDToName(library.ErrorID())};
        }
        const std::string id{getLastRead(library)};
        if (id.empty()) {
            displayError("library does not contain a last read epub");
            return 1;
        }
        if (!readEpubInLibrary(id, shareAbs, conf.lineLength)) {
            displayError("last read epub is not in library anymore");
            return 1;
        }
        return 0;
    }
    case CliCommand::add: {
        bool allAdded{true};
        for (int i{2}; i < argc; ++i) {
            try {
                if (!addToLibrary(argv[i], shareAbs)) {
                    displayError(
                            std::string{"epub is already in the library: "}
                            + argv[i]);
                    allAdded = false;
                }
            } catch (const std::exception& e) {
                displayError(std::string{"failed to add "} + argv[i] + ": "
                             + e.what());
                allAdded = false;
            }
        }
        return allAdded ? 0 : 1;
    }
    case CliCommand::remove:
        if (!deleteFromLibrary(argv[2], shareAbs)) {
            displayError("epub id was not found or is ambiguous");
            return 1;
        }
        return 0;
    case CliCommand::list:
        listLibrary(shareAbs);
        return 0;
    case CliCommand::read:
        if (!readEpubInLibrary(argv[2], shareAbs, conf.lineLength)) {
            displayError("epub id was not found or is ambiguous");
            return 1;
        }
        return 0;
    case CliCommand::setLineLength: {
        int chars{0};
        const std::string_view value{argv[2]};
        const auto [end, error]{std::from_chars(
                value.data(), value.data() + value.size(), chars)};
        if (error != std::errc{} || end != value.data() + value.size()
            || chars <= 0) {
            displayError("line length must be a positive integer");
            return 1;
        }
        setTeiConfLineLength(teiConfAbs, chars);
        return 0;
    }
    default:
        throw std::logic_error{"unrecognized CLI command"};
    }
}

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
    std::cout << "Usage:\n";
    resetBoldColorIfTerm(stdout);
    std::cout << "  tei [OPTIONS] [COMMAND] [ARGS...]\n\n";

    std::cout << "When ran without any arguments, tei reads the last-read "
                 "epub.\n\n";

    boldColorIfTerm(stdout, blueFG);
    std::cout << "Options:\n";
    resetBoldColorIfTerm(stdout);
    printAligned("  -h, --help", "Display this help message");
    std::cout << '\n';

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "Commands:\n";
    resetBoldColorIfTerm(stdout);
    printAligned("  add <file>...", "Add .epub files to library");
    printAligned("  rm, remove, delete <id-prefix>",
                 "Remove epub from library");
    printAligned("  ls, list", "List info of epubs in library");
    printAligned("  read <id-prefix>", "Read epub already in library");
    printAligned("  set-line-length <chars>",
                 "Set the persistent maximum line length");
    std::cout << '\n';

    std::cout << "All full ids can be substituted with unambiguous "
                 "prefixes. (e.g. a6ce475b738e1cd7def7ed1e958426b3 can be "
                 "shortened to a6c.)\n\n";

    boldColorIfTerm(stdout, magentaFG);
    std::cout << "Keybinds:\n";
    resetBoldColorIfTerm(stdout);
    printAligned("  q", "Quit");
    printAligned("  t, <Tab>, <Esc>", "Toggle table of contents");
    printAligned("  <Enter>, <Left Click>",
                 "Open selected table of contents entry");
    printAligned("  h, b, <PgUp>, <Left Arrow>", "Page up");
    printAligned("  l, f, <PgDn>, <Right Arrow>", "Page down");
    printAligned("  <Space>", "Page down");
    printAligned("  u", "Half-page up");
    printAligned("  d", "Half-page down");
    printAligned("  k, <Up Arrow>", "One line up");
    printAligned("  j, <Down Arrow>", "One line down");
    printAligned("  g, <Home>", "Jump to chapter beginning");
    printAligned("  G, <End>", "Jump to chapter end");
    printAligned("  <Left Click>", "Page up/down based on cursor position");
    printAligned("  <Wheel Up>", "Scroll up");
    printAligned("  <Wheel Down>", "Scroll down");
}

void listLibrary(const fs::path& shareAbs) {
    const fs::path teiLibraryAbs{shareAbs / "tei/library.xml"};
    XMLDocument teiLibrary{};
    if (teiLibrary.LoadFile(teiLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error loading library file: "}
                + XMLDocument::ErrorIDToName(teiLibrary.ErrorID())};
    }

    XMLElement* const libraryRoot{teiLibrary.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }

    struct Row {
        std::string id;
        std::string title;
        std::string author;
    };
    std::vector<Row> rows;

    for (XMLElement* epub{libraryRoot->FirstChildElement("epub")};
         epub != nullptr; epub = epub->NextSiblingElement("epub")) {
        const char* const id{epub->Attribute("id")};
        if (id == nullptr) {
            throw std::runtime_error{
                    "`<epub>` element missing `id` attribute"};
        }

        const fs::path epubRootAbs{shareAbs / "tei/extracted_epubs" / id};
        const fs::path opfAbs{epubRootAbs / getOPFRel(epubRootAbs)};
        XMLDocument opf{};
        opf.LoadFile(opfAbs.c_str());
        if (opf.Error()) {
            throw std::runtime_error{opf.ErrorStr()};
        }
        const XMLElement* const metadata{getMetadata(opf)};
        rows.emplace_back(id, getTitle(metadata), getAuthor(metadata));
    }

    if (rows.empty()) {
        boldColorIfTerm(stdout, redFG);
        std::cout << "Library is empty";
        resetBoldColorIfTerm(stdout);
        std::cout << '\n';
        return;
    }

    const std::string lastReadId{getLastRead(teiLibrary)};
    std::ranges::sort(rows, {}, &Row::title);

    for (const auto& r : rows) {
        boldColorIfTerm(stdout, magentaFG);
        std::cout << r.title;
        resetBoldColorIfTerm(stdout);
        std::cout << " - ";
        boldColorIfTerm(stdout, blueFG);
        std::cout << r.author;
        resetBoldColorIfTerm(stdout);
        std::cout << " - ";
        boldColorIfTerm(stdout, greenFG);
        std::cout << r.id;
        resetBoldColorIfTerm(stdout);
        std::cout << '\n';
    }

    const auto lastRead{std::ranges::find(rows, lastReadId, &Row::id)};
    if (lastRead == rows.end()) {
        boldColorIfTerm(stdout, redFG);
        std::cout << "Last read epub no longer in library";
        resetBoldColorIfTerm(stdout);
        std::cout << '\n';
        return;
    }

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "Last read:";
    resetBoldColorIfTerm(stdout);
    std::cout << ' ';
    boldColorIfTerm(stdout, magentaFG);
    std::cout << lastRead->title;
    resetBoldColorIfTerm(stdout);
    std::cout << '\n';
}
