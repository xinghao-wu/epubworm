#include "cli.hpp"
#include "epub_parser.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

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
    std::cout << "Usage:\n";
    resetBoldColorIfTerm(stdout);
    std::cout << "  tei [OPTIONS] [COMMAND] [ARGS...]\n";
    std::cout << "  tei\n";
    std::cout << '\n';

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
    printAligned("  add <file>...", "Add epub files to library");
    printAligned("  remove, rm, delete <id>", "Remove epub from library");
    printAligned("  list, ls", "List library's epubs' id, title, author");
    printAligned("  read <id>", "Read epub from library");
    std::cout << '\n';

    std::cout
            << "All full ids can be substituted with unambiguous prefixes.\n";
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
        std::cout << "library is empty\n";
        return;
    }

    constexpr std::string_view idLabel{"ID"};
    constexpr std::string_view titleLabel{"TITLE"};
    constexpr std::string_view authorLabel{"AUTHOR"};

    std::size_t idW{idLabel.size()};
    std::size_t titleW{titleLabel.size()};
    for (const auto& r : rows) {
        idW = std::max(idW, r.id.size());
        titleW = std::max(titleW, r.title.size());
    }

    std::cout << idLabel << std::string(idW - idLabel.size() + 2, ' ')
              << titleLabel << std::string(titleW - titleLabel.size() + 2, ' ')
              << authorLabel << '\n';

    for (const auto& r : rows) {
        std::cout << r.id << std::string(idW - r.id.size() + 2, ' ') << r.title
                  << std::string(titleW - r.title.size() + 2, ' ') << r.author
                  << '\n';
    }
}
