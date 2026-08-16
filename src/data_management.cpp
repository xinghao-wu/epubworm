#include "data_management.hpp"
#include "epub_parser.hpp"
#include "miniz_cpp.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

void unzip(const fs::path& archiveAbs, const fs::path& destinationAbs) {
    miniz_cpp::zip_file archive{archiveAbs.string()};
    archive.extractall(destinationAbs.string());
}

void initConf(const fs::path& mncConfAbs) {
    XMLDocument mncConf{};
    mncConf.InsertFirstChild(mncConf.NewDeclaration());
    mncConf.InsertEndChild(mncConf.NewElement("conf"));

    fs::create_directories(mncConfAbs.parent_path());
    if (mncConf.SaveFile(mncConfAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving conf file: "}
                + XMLDocument::ErrorIDToName(mncConf.ErrorID())};
    }
}

void initLibrary(const fs::path& mncLibraryAbs) {
    XMLDocument mncLibrary{};
    mncLibrary.InsertFirstChild(mncLibrary.NewDeclaration());
    XMLElement* const libraryRoot{mncLibrary.NewElement("library")};
    mncLibrary.InsertEndChild(libraryRoot);
    XMLElement* const lastRead{mncLibrary.NewElement("last-read")};
    lastRead->SetAttribute("id", "");
    libraryRoot->InsertEndChild(lastRead);

    fs::create_directories(mncLibraryAbs.parent_path());
    if (mncLibrary.SaveFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }
}

ConfOpts readMncConf(const fs::path& mncConfAbs) {
    XMLDocument mncConf{};
    mncConf.LoadFile(mncConfAbs.c_str());

    if (mncConf.Error()) {
        throw std::runtime_error{mncConf.ErrorStr()};
    }

    XMLElement* const rootElem{mncConf.FirstChildElement("conf")};
    if (rootElem == nullptr) {
        throw std::runtime_error{
                "root element `<conf>` missing in config file"};
    }

    // Write a default value for unfound options to support adding future
    // conf options without making users edit config file every time.
    if (rootElem->FirstChildElement("line-length") == nullptr) {
        rootElem->InsertEndChild(mncConf.NewElement("line-length"));
    }

    XMLElement* const lineLength{rootElem->FirstChildElement("line-length")};
    if (lineLength->Attribute("chars") == nullptr) {
        lineLength->SetAttribute("chars", 55);
    }

    int chars{0};
    lineLength->QueryIntAttribute("chars", &chars);
    if (chars <= 0) {
        throw std::runtime_error{
                "conf option `<line-length>` has invalid value"};
    }

    if (mncConf.SaveFile(mncConfAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving conf file: "}
                + XMLDocument::ErrorIDToName(mncConf.ErrorID())};
    }

    return {chars};
}

std::string getTruncatedSHA256Sum(const fs::path& fileAbs) {
    const std::string sha256{execute(std::vector<std::string>{
            "shasum", "-a", "256", fileAbs.string()})};
    return sha256.substr(0, 32); // 128 bits = 32 hex chars
}

XMLElement* findEpubById(XMLElement* libraryRoot, std::string_view idPrefix) {
    XMLElement* match{nullptr};
    for (XMLElement* epub{libraryRoot->FirstChildElement("epub")};
         epub != nullptr; epub = epub->NextSiblingElement("epub")) {
        if (const char* const epubId{epub->Attribute("id")};
            epubId != nullptr
            && std::string_view{epubId}.starts_with(idPrefix)) {
            if (match != nullptr) {
                return nullptr; // ambiguous prefix
            }
            match = epub;
        }
    }
    return match;
}

std::pair<std::string, EpubProg> queryEpubElem(const XMLElement* epub,
                                               const fs::path& shareAbs) {
    const char* const id{epub->Attribute("id")};
    if (id == nullptr) {
        throw std::runtime_error{"epub entry missing `id` attribute"};
    }

    EpubProg prog{};
    double chapterProg{};
    const XMLError progErr{
            epub->QueryDoubleAttribute("chapter-progress", &chapterProg)};
    if (progErr == XML_NO_ATTRIBUTE) {
        throw std::runtime_error{
                "epub entry missing `chapter-progress` attribute"};
    }
    if (progErr != XML_SUCCESS) {
        throw std::runtime_error{
                "epub entry has invalid `chapter-progress` attribute"};
    }
    prog.chapterProg = chapterProg;

    if (const char* const openedChapter{epub->Attribute("opened-chapter")};
        openedChapter != nullptr && openedChapter[0] != '\0') {
        prog.chapterAbs =
                shareAbs / "mnc/extracted_epubs" / id / openedChapter;
    }

    return {id, prog};
}

void writeProgress(XMLElement* epub, const EpubProg& prog,
                   const fs::path& shareAbs) {
    const char* const id{epub->Attribute("id")};
    if (id == nullptr) {
        throw std::runtime_error{"epub entry missing `id` attribute"};
    }

    epub->SetAttribute("chapter-progress", prog.chapterProg);

    if (prog.chapterAbs.empty()) {
        epub->SetAttribute("opened-chapter", "");
        return;
    }

    const fs::path epubRootAbs{shareAbs / "mnc/extracted_epubs" / id};
    const fs::path openedChapter{
            prog.chapterAbs.lexically_relative(epubRootAbs)};
    epub->SetAttribute("opened-chapter", openedChapter.string().c_str());
}

void setLastRead(XMLDocument& libraryDoc, std::string_view id) {
    XMLElement* const libraryRoot{libraryDoc.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }
    XMLElement* const lastRead{libraryRoot->FirstChildElement("last-read")};
    if (lastRead == nullptr) {
        throw std::runtime_error{
                "`<last-read>` element missing in library file"};
    }
    lastRead->SetAttribute("id", std::string{id}.c_str());
}

std::string getLastRead(const XMLDocument& libraryDoc) {
    const XMLElement* const libraryRoot{
            libraryDoc.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }
    const XMLElement* const lastRead{
            libraryRoot->FirstChildElement("last-read")};
    if (lastRead == nullptr) {
        throw std::runtime_error{
                "`<last-read>` element missing in library file"};
    }
    const char* const id{lastRead->Attribute("id")};
    if (id == nullptr) {
        throw std::runtime_error{
                "`<last-read>` element missing `id` attribute"};
    }
    return id;
}

bool addToLibrary(const fs::path& zippedEpubAbs, const fs::path& shareAbs) {
    const std::string id{getTruncatedSHA256Sum(zippedEpubAbs)};

    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    XMLDocument mncLibrary{};
    if (mncLibrary.LoadFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error loading library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    XMLElement* const libraryRoot{mncLibrary.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }

    // Check if epub already in library.
    if (findEpubById(libraryRoot, id) != nullptr) {
        return false;
    }

    // Extract epub to its directory.
    const fs::path extractDest{shareAbs / "mnc/extracted_epubs" / id};
    fs::create_directories(extractDest);
    unzip(zippedEpubAbs, extractDest);

    // Add new `<epub>` entry.
    XMLElement* const epubElem{mncLibrary.NewElement("epub")};
    epubElem->SetAttribute("id", id.c_str());
    epubElem->SetAttribute("opened-chapter", "");
    epubElem->SetAttribute("chapter-progress", 0.0);
    libraryRoot->InsertEndChild(epubElem);

    // Update `<last-read>` to the new id.
    setLastRead(mncLibrary, id);

    if (mncLibrary.SaveFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    return true;
}

bool deleteFromLibrary(std::string_view idPrefix, const fs::path& shareAbs) {
    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    XMLDocument mncLibrary{};
    if (mncLibrary.LoadFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error loading library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    XMLElement* const libraryRoot{mncLibrary.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }

    XMLElement* const lastRead{libraryRoot->FirstChildElement("last-read")};
    if (lastRead == nullptr) {
        throw std::runtime_error{
                "`<last-read>` element missing in library file"};
    }

    XMLElement* const epubElem{findEpubById(libraryRoot, idPrefix)};
    if (epubElem == nullptr) {
        return false;
    }

    const char* const id{epubElem->Attribute("id")};

    // Delete the extracted epub directory.
    fs::remove_all(shareAbs / "mnc/extracted_epubs" / id);

    // Reset `<last-read>` if it referenced the removed epub.
    if (const char* const lastReadId{lastRead->Attribute("id")};
        lastReadId != nullptr && std::string_view{lastReadId} == id) {
        lastRead->SetAttribute("id", "");
    }

    // Remove the `<epub>` entry.
    libraryRoot->DeleteChild(epubElem);

    if (mncLibrary.SaveFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    return true;
}

bool readEpubInLibrary(std::string_view idPrefix, const fs::path& shareAbs,
                       int desiredMaxLen) {
    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    XMLDocument mncLibrary{};
    if (mncLibrary.LoadFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error loading library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    XMLElement* const libraryRoot{mncLibrary.FirstChildElement("library")};
    if (libraryRoot == nullptr) {
        throw std::runtime_error{
                "root element `<library>` missing in library file"};
    }

    XMLElement* const epubElem{findEpubById(libraryRoot, idPrefix)};
    if (epubElem == nullptr) {
        return false;
    }

    const auto [id, iniProg]{queryEpubElem(epubElem, shareAbs)};
    const fs::path epubRootAbs{shareAbs / "mnc/extracted_epubs" / id};

    useSystemLocale();
    enableRawMode();
    const EpubProg exitProg{displayEpub(iniProg, epubRootAbs, desiredMaxLen)};

    writeProgress(epubElem, exitProg, shareAbs);
    setLastRead(mncLibrary, id);

    if (mncLibrary.SaveFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    return true;
}

void listLibrary(const fs::path& shareAbs) {
    const fs::path mncLibraryAbs{shareAbs / "mnc/library.xml"};
    XMLDocument mncLibrary{};
    if (mncLibrary.LoadFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error loading library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }

    XMLElement* const libraryRoot{mncLibrary.FirstChildElement("library")};
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

        const fs::path epubRootAbs{shareAbs / "mnc/extracted_epubs" / id};
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
