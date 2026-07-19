#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "tinyxml2.hpp"
#include "tui.hpp"
#include "percent_encoding_decode.hpp"
#include "epub_parser.hpp"

using namespace tinyxml2;
namespace fs = std::filesystem;
using TocData = std::vector<std::pair<std::string, fs::path>>;

fs::path getOPFRel(const fs::path& epubRootAbs) {
    const fs::path containerAbs {epubRootAbs / "META-INF/container.xml"};

    XMLDocument container {};
    container.LoadFile(containerAbs.c_str());

    if (container.Error()) {
        throw std::runtime_error{container.ErrorStr()};
    }

    return container.FirstChildElement("container")
                    ->FirstChildElement("rootfiles")
                    ->FirstChildElement("rootfile")
                    ->Attribute("full-path");
}

const XMLElement* getMetadata(const XMLDocument& opf) {
    return opf.FirstChildElement("package")->FirstChildElement("metadata");
}

std::string getTitle(const XMLElement* metadata) {
    return metadata->FirstChildElement("dc:title")->GetText();
}

std::string getAuthor(const XMLElement* metadata) {
    return metadata->FirstChildElement("dc:creator")->GetText();
}

const char* getHrefFromID(const XMLElement* manifest, std::string_view id) {
    for (const XMLElement* item {manifest->FirstChildElement("item")};
            item; item = item->NextSiblingElement("item")) {
        if (item->Attribute("id") == id) {
            return item->Attribute("href");
        }
    }
    throw std::runtime_error{"ID not found in manifest"};
}

std::vector<fs::path> getSpine(const XMLDocument& opf) {
    const XMLElement* spine = opf.FirstChildElement("package")
                                 ->FirstChildElement("spine");
    const XMLElement* manifest = opf.FirstChildElement("package")
                                    ->FirstChildElement("manifest");
    std::vector<fs::path> result {};

    const char* tocID = spine->Attribute("toc");
    const char* tocHref = getHrefFromID(manifest, tocID);
    result.push_back(tocHref);

    for (const XMLElement* itemref {spine->FirstChildElement("itemref")};
            itemref; itemref = itemref->NextSiblingElement("itemref")) {
        const char* idref = itemref->Attribute("idref");
        std::string href = getHrefFromID(manifest, idref);
        decodePercentEncoding(href);

        const char* linear = itemref->Attribute("linear");
        if (linear && std::string_view{linear} == "no") {
            continue;
        }

        result.push_back(href);
    }

    return result;
}

void collectNavPoints(const XMLElement* parent, TocData& tocData,
                      const std::string& prefix) {
    for (const XMLElement* navPoint = parent->FirstChildElement("navPoint");
            navPoint; navPoint = navPoint->NextSiblingElement("navPoint")) {

        const char* name = navPoint->FirstChildElement("navLabel")
                                   ->FirstChildElement("text")->GetText();

        std::string srcFilePathRel = navPoint->FirstChildElement("content")
                                             ->Attribute("src");
        decodePercentEncoding(srcFilePathRel);

        tocData.emplace_back(prefix + name, srcFilePathRel);
        collectNavPoints(navPoint, tocData, prefix + "    ");
    }
}

TocData getTOC(const fs::path& tocAbs) {
    XMLDocument toc {};
    toc.LoadFile(tocAbs.c_str());

    if (toc.Error()) {
        throw std::runtime_error{toc.ErrorStr()};
    }

    const XMLElement* navMap = toc.FirstChildElement("ncx")
                                  ->FirstChildElement("navMap");

    TocData result {};
    collectNavPoints(navMap, result);

    return result;
}

void parseContentElem(const XMLElement* parent, std::string& out,
                      const fs::path& chapterAbs) {
    for (const XMLNode* childNode = parent->FirstChild();
            childNode; childNode = childNode->NextSibling()) {
        if (const XMLText* childText = childNode->ToText()) {
            out += childText->Value();
        }
        if (const XMLElement* childElem = childNode->ToElement()) {
            const std::string_view name {childElem->Name()};
            if (name == "b" || name == "strong") {
                out += esc + bold;
                parseContentElem(childElem, out, chapterAbs);
                out += esc + resetBold;
            }
            else if (name == "i" || name == "em") {
                out += esc + italic;
                parseContentElem(childElem, out, chapterAbs);
                out += esc + resetItalic;
            }
            else if (name == "br") {
                out += '\n';
            }
            else if (name == "h1" || name == "h2" || name == "h3" 
                     || name == "h4" || name == "h5" || name == "h6") {
                out += esc + bold;
                out += esc + yellowFG;
                parseContentElem(childElem, out, chapterAbs);
                out += esc + resetBold;
                out += esc + resetFG;
                out += "\n\n";
            }
            else if (name == "p" || name == "li") {
                parseContentElem(childElem, out, chapterAbs);
                out += "\n\n";
            }
            else if (name == "image" || name == "img") {
                const std::string_view imgAttributeName {
                        (name == "image") ? "xlink:href" : "src"};
                std::string imgPathAbs {chapterAbs.parent_path() 
                        / childElem->Attribute(imgAttributeName.data())};
                decodePercentEncoding(imgPathAbs);

                try {
                    displayImg(imgPathAbs, out);
                    out += '\n';
                }
                catch (const std::runtime_error& e) {
                    if (std::string_view{e.what()} == "Unable to open file") {
                        out += esc + bold; 
                        out += "[image reference in epub "
                               "points to nonexistent file]";
                        out += esc + resetBold + "\n\n";
                    }
                    else {
                        throw;
                    }
                }
            }
            else {
                parseContentElem(childElem, out, chapterAbs);
            }
        }
    }
}

void parseChapter(const fs::path& chapterAbs, std::string& out) {
    XMLDocument chapter {};
    chapter.LoadFile(chapterAbs.c_str());
    if (chapter.Error()) {
        throw std::runtime_error{chapter.ErrorStr()};
    }

    const XMLElement* const body {chapter.FirstChildElement("html")
                                         ->FirstChildElement("body")};

    parseContentElem(body, out, chapterAbs);
    out.pop_back(); // remove extraneous newline
    out += esc + bold;
    out += esc + redFG;
    out += "---";
    out += esc + resetBold;
    out += esc + resetFG;
}

void dumpEpub(const fs::path& epubRootAbs, std::string& out) {
    const fs::path opfAbs {epubRootAbs / getOPFRel(epubRootAbs)};
    XMLDocument opf {};
    opf.LoadFile(opfAbs.c_str());
    if (opf.Error()) {
        throw std::runtime_error{opf.ErrorStr()};
    }

    const std::vector<fs::path> spine {getSpine(opf)};

    for (int i {1}; i < std::ssize(spine); ++i) {
        parseChapter(opfAbs.parent_path() / spine.data()[i], out);
        out += "\n\n";
    }
}

void expandEllipsesAndTabs(std::string& str) {
    findAndReplaceAll(str, "…", "...");
    findAndReplaceAll(str, "\t", "    ");
}
