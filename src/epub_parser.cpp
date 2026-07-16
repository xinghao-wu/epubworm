#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "tinyxml2.hpp"
#include "tui.hpp"
#include "epub_parser.hpp"

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
        const char* href = getHrefFromID(manifest, idref);

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

        const char* srcFile = navPoint->FirstChildElement("content")
                                      ->Attribute("src");

        tocData.emplace_back(prefix + name, srcFile);
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
                out += esc + "[1m";
                parseContentElem(childElem, out, chapterAbs);
                out += esc + "[22m";
            }
            else if (name == "i" || name == "em") {
                out += esc + "[3m";
                parseContentElem(childElem, out, chapterAbs);
                out += esc + "[23m";
            }
            else if (name == "br") {
                out += '\n';
            }
            else if (name == "h1" || name == "h2" || name == "h3" 
                     || name == "h4" || name == "h5" || name == "h6") {
                out += esc + "[1m" + "# ";
                parseContentElem(childElem, out, chapterAbs);
                out += esc + "[22m" + "\n\n";
            }
            else if (name == "p") {
                parseContentElem(childElem, out, chapterAbs);
                out += "\n\n";
            }
            else if (name == "image") {
                displayImg(chapterAbs.parent_path() 
                           / childElem->Attribute("xlink:href"), out);
                out += '\n';
            }
            else if (name == "img") {
                displayImg(chapterAbs.parent_path() 
                           / childElem->Attribute("src"), out);
                out += '\n';
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
}
