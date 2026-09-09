#include "epub_parser.hpp"
#include "percent_encoding_decode.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

namespace {
enum class TextAlignment {
    inherit,
    left,
    center,
    right,
};

constexpr bool isHTMLWhitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f';
}

std::string_view trim(std::string_view str) {
    while (!str.empty() && isHTMLWhitespace(str.front())) {
        str.remove_prefix(1);
    }
    while (!str.empty() && isHTMLWhitespace(str.back())) {
        str.remove_suffix(1);
    }
    return str;
}

std::string lowerASCII(std::string_view str) {
    std::string result{str};
    for (char& ch : result) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch + ('a' - 'A'));
        }
    }
    return result;
}

bool hasNumericSuffix(std::string_view str, std::string_view prefix) {
    if (!str.starts_with(prefix) || str.size() == prefix.size()) {
        return false;
    }
    return std::ranges::all_of(str.substr(prefix.size()),
                               [](char ch) { return ch >= '0' && ch <= '9'; });
}

TextAlignment parseAlignmentValue(std::string_view value) {
    const std::string lowerValue{lowerASCII(trim(value))};
    if (lowerValue == "center") return TextAlignment::center;
    if (lowerValue == "right") return TextAlignment::right;
    if (lowerValue == "left" || lowerValue == "justify") {
        return TextAlignment::left;
    }
    return TextAlignment::inherit;
}

TextAlignment getClassAlignment(const XMLElement* elem) {
    const char* classAttr{elem->Attribute("class")};
    if (classAttr == nullptr) return TextAlignment::inherit;

    bool centerFound{false};
    bool rightFound{false};
    bool leftFound{false};
    std::string_view classes{classAttr};
    while (!classes.empty()) {
        while (!classes.empty() && isHTMLWhitespace(classes.front())) {
            classes.remove_prefix(1);
        }
        const std::size_t tokenEnd{classes.find_first_of(" \t\n\r\f")};
        const std::string token{lowerASCII(classes.substr(0, tokenEnd))};

        centerFound = centerFound || token == "center" || token == "centerp"
                      || token == "centered" || token == "centre"
                      || token == "centred" || token == "text-center"
                      || token == "align-center" || token == "align-center-rw"
                      || token == "has-text-align-center"
                      || token == "separator" || token == "ornamental-break"
                      || token == "ornamental-break-as-text"
                      || hasNumericSuffix(token, "center")
                      || hasNumericSuffix(token, "centre");
        rightFound = rightFound || token == "right" || token == "rightp"
                     || token == "right-aligned" || token == "text-right"
                     || token == "align-right"
                     || hasNumericSuffix(token, "right");
        leftFound = leftFound || token == "left" || token == "text-left"
                    || token == "align-left" || token == "justify"
                    || token == "justified" || token == "text-justify"
                    || token == "align-justify";

        if (tokenEnd == std::string_view::npos) break;
        classes.remove_prefix(tokenEnd + 1);
    }

    if (leftFound) return TextAlignment::left;
    if (rightFound) return TextAlignment::right;
    if (centerFound) return TextAlignment::center;
    return TextAlignment::inherit;
}

TextAlignment getInlineStyleAlignment(const XMLElement* elem) {
    const char* styleAttr{elem->Attribute("style")};
    if (styleAttr == nullptr) return TextAlignment::inherit;

    TextAlignment result{TextAlignment::inherit};
    bool importantResult{false};
    std::string_view declarations{styleAttr};
    while (!declarations.empty()) {
        const std::size_t declarationEnd{declarations.find(';')};
        const std::string_view declaration{
                declarations.substr(0, declarationEnd)};
        const std::size_t colon{declaration.find(':')};
        if (colon != std::string_view::npos
            && lowerASCII(trim(declaration.substr(0, colon)))
                       == "text-align") {
            std::string value{lowerASCII(trim(declaration.substr(colon + 1)))};
            constexpr std::string_view important{"!important"};
            bool isImportant{false};
            if (value.ends_with(important)) {
                value = trim(std::string_view{value}.substr(
                        0, value.size() - important.size()));
                isImportant = true;
            }
            const TextAlignment alignment{parseAlignmentValue(value)};
            if (alignment != TextAlignment::inherit
                && (isImportant || !importantResult)) {
                result = alignment;
                importantResult = isImportant;
            }
        }

        if (declarationEnd == std::string_view::npos) break;
        declarations.remove_prefix(declarationEnd + 1);
    }
    return result;
}

TextAlignment getElementAlignment(const XMLElement* elem,
                                  TextAlignment inherited) {
    TextAlignment result{getClassAlignment(elem)};
    if (std::string_view{elem->Name()} == "center") {
        result = TextAlignment::center;
    }
    if (const char* alignAttr{elem->Attribute("align")}) {
        const TextAlignment attrAlignment{parseAlignmentValue(alignAttr)};
        if (attrAlignment != TextAlignment::inherit) result = attrAlignment;
    }
    const TextAlignment styleAlignment{getInlineStyleAlignment(elem)};
    if (styleAlignment != TextAlignment::inherit) result = styleAlignment;
    return result == TextAlignment::inherit ? inherited : result;
}

bool hasTextContent(const XMLNode* parent) {
    for (const XMLNode* child{parent->FirstChild()}; child != nullptr;
         child = child->NextSibling()) {
        if (const XMLText* text = child->ToText()) {
            const std::string_view value{text->Value()};
            if (std::ranges::any_of(value, [](char ch) {
                    return !isHTMLWhitespace(ch);
                })) {
                return true;
            }
        }
        if (child->ToElement() != nullptr && hasTextContent(child))
            return true;
    }
    return false;
}

bool isBlockElement(std::string_view name) {
    return name == "p" || name == "li" || name == "pre" || name == "table"
           || name == "caption" || name == "hr" || name == "h1" || name == "h2"
           || name == "h3" || name == "h4" || name == "h5" || name == "h6"
           || name == "div" || name == "section" || name == "article"
           || name == "aside" || name == "main" || name == "header"
           || name == "footer" || name == "blockquote" || name == "center";
}

bool handlesAlignment(std::string_view name) {
    return isBlockElement(name) && name != "caption";
}

bool hasAlignmentBlockDescendant(const XMLNode* parent) {
    for (const XMLElement* child{parent->FirstChildElement()};
         child != nullptr; child = child->NextSiblingElement()) {
        if (handlesAlignment(child->Name())
            || hasAlignmentBlockDescendant(child)) {
            return true;
        }
    }
    return false;
}

bool isAlignmentContainer(std::string_view name) {
    return name == "section" || name == "article" || name == "aside"
           || name == "main" || name == "header" || name == "footer"
           || name == "blockquote" || name == "center";
}

void appendAlignmentBegin(std::string& out, TextAlignment alignment) {
    if (alignment == TextAlignment::center) out += centerAlignBegin;
    if (alignment == TextAlignment::right) out += rightAlignBegin;
}

void appendAlignmentEnd(std::string& out, TextAlignment alignment) {
    if (alignment == TextAlignment::center) out += centerAlignEnd;
    if (alignment == TextAlignment::right) out += rightAlignEnd;
}

const std::string& getHeadingColor(std::string_view name) {
    if (name == "h1") return yellowFG;
    if (name == "h2") return magentaFG;
    if (name == "h3") return blueFG;
    if (name == "h4") return cyanFG;
    if (name == "h5") return lightGrayFG;
    return lightGrayFG;
}

bool isHeading(std::string_view name) {
    return name == "h1" || name == "h2" || name == "h3" || name == "h4"
           || name == "h5" || name == "h6";
}

struct TextStyle {
    bool bold{};
    bool italic{};
    std::string_view foreground{};
};

void appendStyleTransition(std::string& out, const TextStyle& current,
                           const TextStyle& next) {
    if (current.bold != next.bold) {
        out += esc + (next.bold ? bold : resetBold);
    }
    if (current.italic != next.italic) {
        out += esc + (next.italic ? italic : resetItalic);
    }
    if (current.foreground != next.foreground) {
        if (!current.foreground.empty()) out += esc + resetFG;
        if (!next.foreground.empty()) {
            out += esc;
            out += next.foreground;
        }
    }
}

std::string getTableCellSeparator(const TextStyle& style) {
    TextStyle separatorStyle{style};
    separatorStyle.bold = false;
    separatorStyle.italic = false;
    separatorStyle.foreground = cyanFG;

    std::string separator{};
    appendStyleTransition(separator, style, separatorStyle);
    separator += " | ";
    appendStyleTransition(separator, separatorStyle, style);
    return separator;
}

void parseContentElemImpl(const XMLElement* parent, std::string& out,
                          const fs::path& chapterAbs,
                          TextAlignment inheritedAlignment, TextStyle style);
} // namespace

fs::path getOPFRel(const fs::path& epubRootAbs) {
    const fs::path containerAbs{epubRootAbs / "META-INF/container.xml"};

    XMLDocument container{};
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
    for (const XMLElement* item{manifest->FirstChildElement("item")};
         item != nullptr; item = item->NextSiblingElement("item")) {
        if (item->Attribute("id") == id) {
            return item->Attribute("href");
        }
    }
    throw std::runtime_error{"element matching provided `<spine>` ID not "
                             "found in `<manifest>`"};
}

std::vector<fs::path> getSpine(const XMLDocument& opf) {
    const XMLElement* spine{
            opf.FirstChildElement("package")->FirstChildElement("spine")};
    const XMLElement* manifest{
            opf.FirstChildElement("package")->FirstChildElement("manifest")};
    std::vector<fs::path> result{};

    const char* tocID{spine->Attribute("toc")};
    std::string tocHref{getHrefFromID(manifest, tocID)};
    decodePercentEncoding(tocHref);
    result.emplace_back(tocHref);

    for (const XMLElement* itemref{spine->FirstChildElement("itemref")};
         itemref != nullptr;
         itemref = itemref->NextSiblingElement("itemref")) {

        const char* idref{itemref->Attribute("idref")};
        std::string href{getHrefFromID(manifest, idref)};
        decodePercentEncoding(href);

        const char* linear{itemref->Attribute("linear")};
        if (linear != nullptr && std::string_view{linear} == "no") {
            continue;
        }

        result.emplace_back(href);
    }

    return result;
}

void collectNavPoints(const XMLElement* parent, TocData& tocData,
                      const std::string& prefix) {
    for (const XMLElement* navPoint{parent->FirstChildElement("navPoint")};
         navPoint != nullptr;
         navPoint = navPoint->NextSiblingElement("navPoint")) {

        const char* name{navPoint->FirstChildElement("navLabel")
                                 ->FirstChildElement("text")
                                 ->GetText()};

        std::string srcFilePathRel{
                navPoint->FirstChildElement("content")->Attribute("src")};
        decodePercentEncoding(srcFilePathRel);
        if (srcFilePathRel.contains('#')) {
            srcFilePathRel.resize(srcFilePathRel.find('#'));
        }

        tocData.emplace_back(prefix + name, srcFilePathRel);
        collectNavPoints(navPoint, tocData, prefix + "    ");
    }
}

TocData getTOC(const fs::path& tocAbs) {
    XMLDocument toc{};
    toc.LoadFile(tocAbs.c_str());

    if (toc.Error()) {
        throw std::runtime_error{toc.ErrorStr()};
    }

    const XMLElement* navMap{
            toc.FirstChildElement("ncx")->FirstChildElement("navMap")};

    TocData result{};
    collectNavPoints(navMap, result);

    return result;
}

namespace {
void parseContentElemImpl(const XMLElement* parent, std::string& out,
                          const fs::path& chapterAbs,
                          TextAlignment inheritedAlignment, TextStyle style) {
    for (const XMLNode* childNode{parent->FirstChild()}; childNode != nullptr;
         childNode = childNode->NextSibling()) {
        if (const XMLText* childText = childNode->ToText()) {
            out += childText->Value();
        }
        if (const XMLElement* childElem = childNode->ToElement()) {
            const std::string_view name{childElem->Name()};
            const TextAlignment childAlignment{
                    getElementAlignment(childElem, inheritedAlignment)};
            const bool isBlock{isBlockElement(name)};
            if (isBlock) out += "\n\n";

            if (name == "b" || name == "strong") {
                TextStyle childStyle{style};
                childStyle.bold = true;
                appendStyleTransition(out, style, childStyle);
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, childStyle);
                appendStyleTransition(out, childStyle, style);
            } else if (name == "i" || name == "em") {
                TextStyle childStyle{style};
                childStyle.italic = true;
                appendStyleTransition(out, style, childStyle);
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, childStyle);
                appendStyleTransition(out, childStyle, style);
            } else if (name == "code") {
                TextStyle childStyle{style};
                childStyle.foreground = greenFG;
                appendStyleTransition(out, style, childStyle);
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, childStyle);
                appendStyleTransition(out, childStyle, style);
            } else if (name == "br") {
                out += '\n';
            } else if (name == "hr") {
                TextStyle childStyle{style};
                childStyle.bold = true;
                childStyle.italic = false;
                childStyle.foreground = {};
                out += centerAlignBegin;
                appendStyleTransition(out, style, childStyle);
                out += "***";
                appendStyleTransition(out, childStyle, style);
                out += centerAlignEnd;
            } else if (isHeading(name)) {
                TextStyle childStyle{style};
                childStyle.bold = true;
                childStyle.foreground = getHeadingColor(name);
                out += centerAlignBegin;
                appendStyleTransition(out, style, childStyle);
                parseContentElemImpl(childElem, out, chapterAbs,
                                     TextAlignment::center, childStyle);
                appendStyleTransition(out, childStyle, style);
                out += centerAlignEnd;
            } else if (name == "p" || name == "li" || name == "div"
                       || name == "pre" || name == "table") {
                TextStyle childStyle{style};
                if (name == "pre") childStyle.foreground = yellowFG;
                const bool markAlignment{
                        childAlignment != TextAlignment::left
                        && hasTextContent(childElem)
                        && !hasAlignmentBlockDescendant(childElem)};
                if (markAlignment) appendAlignmentBegin(out, childAlignment);
                appendStyleTransition(out, style, childStyle);
                const std::size_t contentBegin{out.size()};
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, childStyle);
                if (name == "pre") {
                    std::size_t styleEnd{out.size()};
                    while (styleEnd > contentBegin
                           && isHTMLWhitespace(out[styleEnd - 1])) {
                        --styleEnd;
                    }
                    std::string styleTransition{};
                    appendStyleTransition(styleTransition, childStyle, style);
                    out.insert(styleEnd, styleTransition);
                } else {
                    appendStyleTransition(out, childStyle, style);
                }
                if (markAlignment) appendAlignmentEnd(out, childAlignment);
            } else if (name == "tr") {
                out += '\n';
                const std::size_t contentBegin{out.size()};
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, style);
                const std::string separator{getTableCellSeparator(style)};
                if (out.size() >= contentBegin + separator.size()
                    && out.ends_with(separator)) {
                    out.resize(out.size() - separator.size());
                }
            } else if (name == "td" || name == "th") {
                if (!childElem->NoChildren()) {
                    TextStyle childStyle{style};
                    if (name == "th") childStyle.bold = true;
                    appendStyleTransition(out, style, childStyle);
                    parseContentElemImpl(childElem, out, chapterAbs,
                                         childAlignment, childStyle);
                    appendStyleTransition(out, childStyle, style);
                    out += getTableCellSeparator(style);
                }
            } else if (isAlignmentContainer(name)
                       && childAlignment != TextAlignment::left
                       && hasTextContent(childElem)
                       && !hasAlignmentBlockDescendant(childElem)) {
                appendAlignmentBegin(out, childAlignment);
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, style);
                appendAlignmentEnd(out, childAlignment);
            } else if (name == "image" || name == "img") {
                const std::string imgAttributeName{
                        (name == "image") ? "xlink:href" : "src"};
                std::string imgPathAbs{
                        chapterAbs.parent_path()
                        / childElem->Attribute(imgAttributeName.data())};
                decodePercentEncoding(imgPathAbs);

                const std::size_t imageBegin{out.size()};
                try {
                    out += "\n\n";
                    displayImg(imgPathAbs, out);
                    out += "\n\n";
                } catch (const std::runtime_error& e) {
                    // If the image reference points to a nonexistent file or
                    // image is of an unsupported format, its
                    // best to just silently ignore it.
                    if (!std::string_view{e.what()}.contains("Unable to open")
                        && !std::string_view{e.what()}.contains(
                                "Image not of any known type")) {
                        throw;
                    }
                    out.resize(imageBegin);
                }
            } else {
                parseContentElemImpl(childElem, out, chapterAbs,
                                     childAlignment, style);
            }

            if (isBlock) out += "\n\n";
        }
    }
}
} // namespace

void parseContentElem(const XMLElement* parent, std::string& out,
                      const fs::path& chapterAbs) {
    parseContentElemImpl(parent, out, chapterAbs,
                         getElementAlignment(parent, TextAlignment::left), {});
}

void parseChapter(const fs::path& chapterAbs, std::string& out) {
    XMLDocument chapter{};
    chapter.LoadFile(chapterAbs.c_str());

    if (chapter.Error()) {
        throw std::runtime_error{chapter.ErrorStr()};
    }

    const XMLElement* const body{
            chapter.FirstChildElement("html")->FirstChildElement("body")};

    const std::size_t chapterBegin{out.size()};
    parseContentElem(body, out, chapterAbs);

    constexpr std::string_view nonBreakingSpace{"\xC2\xA0"};
    std::size_t firstContent{chapterBegin};
    const std::string_view untrimmed{out};
    while (firstContent < untrimmed.size()) {
        if (untrimmed[firstContent] == '\n' || untrimmed[firstContent] == ' '
            || untrimmed[firstContent] == '\t') {
            ++firstContent;
        } else if (untrimmed.substr(firstContent, nonBreakingSpace.size())
                   == nonBreakingSpace) {
            firstContent += nonBreakingSpace.size();
        } else {
            break;
        }
    }

    out.erase(chapterBegin, firstContent - chapterBegin);

    while (out.size() > chapterBegin) {
        if (out.back() == '\n' || out.back() == ' ' || out.back() == '\t') {
            out.pop_back();
        } else if (out.size() - chapterBegin >= nonBreakingSpace.size()
                   && out.ends_with(nonBreakingSpace)) {
            out.resize(out.size() - nonBreakingSpace.size());
        } else {
            break;
        }
    }

    out += '\n';
    out += centerAlignBegin;
    out += esc + bold;
    out += esc + redFG;
    out += "---";
    out += esc + resetBold;
    out += esc + resetFG;
    out += centerAlignEnd;
    out += '\n';
}

void dumpEpub(const fs::path& epubRootAbs, std::string& out) {
    const fs::path opfAbs{epubRootAbs / getOPFRel(epubRootAbs)};
    XMLDocument opf{};
    opf.LoadFile(opfAbs.c_str());

    if (opf.Error()) {
        throw std::runtime_error{opf.ErrorStr()};
    }

    const std::vector<fs::path> spine{getSpine(opf)};

    for (int i{1}; i < std::ssize(spine); ++i) {
        parseChapter(opfAbs.parent_path() / spine.data()[i], out);
    }
}

void expandEllipsesAndTabs(std::string& str) {
    findAndReplaceAll(str, "…", "...");
    findAndReplaceAll(str, "\t", "    ");
}
