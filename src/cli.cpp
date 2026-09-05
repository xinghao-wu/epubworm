#include "cli.hpp"
#include "data_management.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <expected>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

static constexpr int descCol{31};
static constexpr std::string_view mascot{R"(        ||
/\      ||
||   .--||-.
 \\ /  _//_ `\                   ▄
  `|  |-`  \  \   █▀▀█ █▀▀█ █  █ █▀▀█ █   █ █▀▀█ █▀▀▄ █▄ ▄█
   /  /    |  |   █▀▀▀ █  █ █  █ █  █ █ █ █ █  █ █    █ ▀ █
  /  /    /  /    ▀▀▀▀ █▀▀▀ ▀▀▀▀ ▀▀▀▀ ▀▀ ▀▀ ▀▀▀▀ ▀    ▀   ▀
 |  |    /  /
 \  |   |  (   _
  `"     \  `-'O`\
          '-._.--'
)"};

enum class CliCommand {
    invalid,
    help,
    none,
    add,
    remove,
    list,
    open,
    setLineLength,
};

static void displayError(std::string_view message) {
    boldColorIfTerm(stderr, redFG);
    std::cerr << "Error: ";
    resetBoldColorIfTerm(stderr);
    std::cerr << message << '\n';
}

static void printEpubInfo(const EpubInfo& info,
                          const XMLElement* libraryRoot) {
    boldColorIfTerm(stdout, magentaFG);
    std::cout << info.title;
    resetBoldColorIfTerm(stdout);
    std::cout << " - ";
    boldColorIfTerm(stdout, blueFG);
    std::cout << info.author;
    resetBoldColorIfTerm(stdout);
    std::cout << " | ";
    boldColorIfTerm(stdout, yellowFG);
    std::cout << getUnambiguousEpubIdPrefix(libraryRoot, info.id);
    resetBoldColorIfTerm(stdout);
}

static void printLibraryUpdate(std::string_view label, std::string_view color,
                               const EpubInfo& info,
                               const XMLElement* libraryRoot) {
    boldColorIfTerm(stdout, color);
    std::cout << label;
    resetBoldColorIfTerm(stdout);
    printEpubInfo(info, libraryRoot);
    std::cout << '\n';
}

static void displayDuplicateEpub(std::string_view filePath) {
    boldColorIfTerm(stdout, redFG);
    std::cout << "Epub is already in library: ";
    resetBoldColorIfTerm(stdout);
    std::cout << filePath << '\n';
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
    } else if (command == "open") {
        if (argc == 3) {
            return CliCommand::open;
        }
    } else if (command == "set-line-length") {
        if (argc == 3) {
            return CliCommand::setLineLength;
        }
    } else {
        displayError("unknown command, run epubworm -h for usage");
        return CliCommand::invalid;
    }

    displayError("invalid command argument count, run epubworm -h for usage");
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
    const fs::path configFileAbs{configAbs / "epubworm/conf.xml"};
    const fs::path libraryFileAbs{shareAbs / "epubworm/library.xml"};

    if (!fs::exists(configFileAbs)) {
        initConf(configFileAbs);
    }
    if (!fs::exists(libraryFileAbs)) {
        initLibrary(libraryFileAbs);
    }
    const ConfOpts conf{readConfig(configFileAbs)};

    switch (command) {
    case CliCommand::none: {
        XMLDocument library{};
        if (library.LoadFile(libraryFileAbs.c_str()) != XML_SUCCESS) {
            throw std::runtime_error{
                    std::string{"error loading library file: "}
                    + XMLDocument::ErrorIDToName(library.ErrorID())};
        }
        const std::string id{getLastRead(library)};
        if (id.empty()) {
            displayError("no last-read epub, or its no longer in library");
            return 1;
        }
        if (!readEpubInLibrary(id, shareAbs, conf.lineLength)) {
            displayError("no last-read epub, or its no longer in library");
            return 1;
        }
        return 0;
    }
    case CliCommand::add: {
        std::vector<std::expected<EpubInfo, LibraryUpdateError>> results{};
        results.reserve(static_cast<std::size_t>(argc - 2));
        std::exception_ptr addError{};
        for (int i{2}; i < argc; ++i) {
            try {
                boldColorIfTerm(stdout, cyanFG);
                std::cout << "Processing: ";
                resetBoldColorIfTerm(stdout);
                std::cout << argv[i] << '\n';

                results.emplace_back(addToLibrary(argv[i], shareAbs));
            } catch (...) {
                addError = std::current_exception();
                break;
            }
        }

        XMLDocument libraryDoc{};
        if (libraryDoc.LoadFile(libraryFileAbs.c_str()) != XML_SUCCESS) {
            throw std::runtime_error{
                    std::string{"error loading library file: "}
                    + XMLDocument::ErrorIDToName(libraryDoc.ErrorID())};
        }
        const XMLElement* const libraryRoot{
                libraryDoc.FirstChildElement("library")};
        if (libraryRoot == nullptr) {
            throw std::runtime_error{
                    "root element `<library>` missing in library file"};
        }

        int argIndex{2};
        for (const auto& result : results) {
            if (!result.has_value()) {
                displayDuplicateEpub(argv[argIndex]);
            } else {
                printLibraryUpdate("Added: ", greenFG, result.value(),
                                   libraryRoot);
            }
            ++argIndex;
        }
        if (addError != nullptr) {
            std::rethrow_exception(addError);
        }
        return 0;
    }
    case CliCommand::remove: {
        const std::expected<EpubInfo, LibraryUpdateError> removed{
                deleteFromLibrary(argv[2], shareAbs)};
        if (!removed.has_value()) {
            displayError("epub with provided id hash not found, or the prefix "
                         "is ambiguous");
            return 1;
        }

        XMLDocument libraryDoc{};
        if (libraryDoc.LoadFile(libraryFileAbs.c_str()) != XML_SUCCESS) {
            throw std::runtime_error{
                    std::string{"error loading library file: "}
                    + XMLDocument::ErrorIDToName(libraryDoc.ErrorID())};
        }
        const XMLElement* const libraryRoot{
                libraryDoc.FirstChildElement("library")};
        if (libraryRoot == nullptr) {
            throw std::runtime_error{
                    "root element `<library>` missing in library file"};
        }
        printLibraryUpdate("Removed: ", greenFG, removed.value(), libraryRoot);
        return 0;
    }
    case CliCommand::list:
        listLibrary(shareAbs);
        return 0;
    case CliCommand::open:
        if (!readEpubInLibrary(argv[2], shareAbs, conf.lineLength)) {
            displayError("epub with provided id hash not found, or the prefix "
                         "is ambiguous");
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
        setConfigLineLength(configFileAbs, chars);
        boldColorIfTerm(stdout, greenFG);
        std::cout << "Line length set to " << chars << '\n';
        resetBoldColorIfTerm(stdout);
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
    std::cout << mascot;

    boldColorIfTerm(stdout, greenFG);
    std::cout << "Usage:\n";
    resetBoldColorIfTerm(stdout);
    std::cout << "  epubworm [command] [args]\n\n";

    boldColorIfTerm(stdout, yellowFG);
    std::cout << "Commands:\n";
    resetBoldColorIfTerm(stdout);
    printAligned("  add <file>...", "Add .epub files to library");
    printAligned("  rm, remove <id hash>", "Remove epub from library");
    printAligned("  open <id hash>", "Open TUI to read epub in library");
    printAligned("  ls, list", "List info of epubs in library");
    printAligned("  set-line-length <chars>",
                 "Set the TUI's max characters per line");
    std::cout << '\n';

    std::cout << "Run without a command (as just epubworm) "
                 "to open the last-read epub.\n\n";
    std::cout
            << "epubworm uses an id hash system similar to git's. Once added "
               "to the library, an epub "
               "file's hash serves as its id to be used in commands. id "
               "hashes are displayed as unambiguous prefixes of at least four "
               "characters. Any unambiguous prefix can be used in place of "
               "a full id hash.\n\n";

    boldColorIfTerm(stdout, magentaFG);
    std::cout << "TUI Keyboard & Mouse Controls:\n";
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
    const fs::path libraryFileAbs{shareAbs / "epubworm/library.xml"};
    XMLDocument libraryDoc{};
    if (libraryDoc.LoadFile(libraryFileAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error loading library file: "}
                + XMLDocument::ErrorIDToName(libraryDoc.ErrorID())};
    }

    XMLElement* const libraryRoot{libraryDoc.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }

    std::vector<EpubInfo> rows;

    for (XMLElement* epub{libraryRoot->FirstChildElement("epub")};
         epub != nullptr; epub = epub->NextSiblingElement("epub")) {
        const char* const id{epub->Attribute("id")};
        if (id == nullptr) {
            throw std::runtime_error{
                    "`<epub>` element missing `id` attribute"};
        }

        rows.emplace_back(getEpubInfo(id, shareAbs));
    }

    if (rows.empty()) {
        boldColorIfTerm(stdout, redFG);
        std::cout << "Library is empty";
        resetBoldColorIfTerm(stdout);
        std::cout << '\n';
        return;
    }

    const std::string lastReadId{getLastRead(libraryDoc)};
    std::ranges::sort(rows, {}, &EpubInfo::title);

    for (const auto& r : rows) {
        printEpubInfo(r, libraryRoot);
        std::cout << '\n';
    }

    const auto lastRead{std::ranges::find(rows, lastReadId, &EpubInfo::id)};
    if (lastRead == rows.end()) {
        boldColorIfTerm(stdout, redFG);
        std::cout << "There's no last-read epub, or its no longer in library";
        resetBoldColorIfTerm(stdout);
        std::cout << '\n';
        return;
    }

    boldColorIfTerm(stdout, greenFG);
    std::cout << "Last read: ";
    resetBoldColorIfTerm(stdout);
    boldColorIfTerm(stdout, magentaFG);
    std::cout << lastRead->title;
    resetBoldColorIfTerm(stdout);
    std::cout << '\n';
}
