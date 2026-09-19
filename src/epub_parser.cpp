#include "epub_parser.hpp"
#include "percent_encoding_decode.hpp"
#include "tinyxml2/tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

namespace {
enum class TextAlignment : std::uint8_t {
  inherit,
  left,
  center,
  right,
};

constexpr auto isHTMLWhitespace(char ch) -> bool {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f';
}

auto trim(std::string_view str) -> std::string_view {
  while (!str.empty() && isHTMLWhitespace(str.front())) {
    str.remove_prefix(1);
  }
  while (!str.empty() && isHTMLWhitespace(str.back())) {
    str.remove_suffix(1);
  }
  return str;
}

auto lowerASCII(std::string_view str) -> std::string {
  std::string result{str};
  for (char& ch : result) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch + ('a' - 'A'));
    }
  }
  return result;
}

auto hasNumericSuffix(std::string_view str, std::string_view prefix) -> bool {
  if (!str.starts_with(prefix) || str.size() == prefix.size()) {
    return false;
  }
  return std::ranges::all_of(str.substr(prefix.size()), [](char ch) -> bool {
    return ch >= '0' && ch <= '9';
  });
}

auto parseAlignmentValue(std::string_view value) -> TextAlignment {
  const std::string lowerValue{lowerASCII(trim(value))};
  if (lowerValue == "center") {
    return TextAlignment::center;
  }
  if (lowerValue == "right") {
    return TextAlignment::right;
  }
  if (lowerValue == "left" || lowerValue == "justify") {
    return TextAlignment::left;
  }
  return TextAlignment::inherit;
}

auto getClassAlignment(const XMLElement* elem) -> TextAlignment {
  const char* classAttr{elem->Attribute("class")};
  if (classAttr == nullptr) {
    return TextAlignment::inherit;
  }

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
                  || token == "has-text-align-center" || token == "separator"
                  || token == "ornamental-break"
                  || token == "ornamental-break-as-text"
                  || hasNumericSuffix(token, "center")
                  || hasNumericSuffix(token, "centre");
    rightFound = rightFound || token == "right" || token == "rightp"
                 || token == "right-aligned" || token == "text-right"
                 || token == "align-right" || hasNumericSuffix(token, "right");
    leftFound = leftFound || token == "left" || token == "text-left"
                || token == "align-left" || token == "justify"
                || token == "justified" || token == "text-justify"
                || token == "align-justify";

    if (tokenEnd == std::string_view::npos) {
      break;
    }
    classes.remove_prefix(tokenEnd + 1);
  }

  if (leftFound) {
    return TextAlignment::left;
  }
  if (rightFound) {
    return TextAlignment::right;
  }
  if (centerFound) {
    return TextAlignment::center;
  }
  return TextAlignment::inherit;
}

auto getInlineStyleAlignment(const XMLElement* elem) -> TextAlignment {
  const char* styleAttr{elem->Attribute("style")};
  if (styleAttr == nullptr) {
    return TextAlignment::inherit;
  }

  TextAlignment result{TextAlignment::inherit};
  bool importantResult{false};
  std::string_view declarations{styleAttr};
  while (!declarations.empty()) {
    const std::size_t declarationEnd{declarations.find(';')};
    const std::string_view declaration{declarations.substr(0, declarationEnd)};
    const std::size_t colon{declaration.find(':')};
    if (colon != std::string_view::npos
        && lowerASCII(trim(declaration.substr(0, colon))) == "text-align") {
      std::string value{lowerASCII(trim(declaration.substr(colon + 1)))};
      constexpr std::string_view important{"!important"};
      bool isImportant{false};
      if (value.ends_with(important)) {
        value = trim(
            std::string_view{value}.substr(0, value.size() - important.size()));
        isImportant = true;
      }
      const TextAlignment alignment{parseAlignmentValue(value)};
      if (alignment != TextAlignment::inherit
          && (isImportant || !importantResult)) {
        result = alignment;
        importantResult = isImportant;
      }
    }

    if (declarationEnd == std::string_view::npos) {
      break;
    }
    declarations.remove_prefix(declarationEnd + 1);
  }
  return result;
}

auto getElementAlignment(const XMLElement* elem, TextAlignment inherited)
    -> TextAlignment {
  TextAlignment result{getClassAlignment(elem)};
  if (std::string_view{elem->Name()} == "center") {
    result = TextAlignment::center;
  }
  if (const char* alignAttr{elem->Attribute("align")}) {
    const TextAlignment attrAlignment{parseAlignmentValue(alignAttr)};
    if (attrAlignment != TextAlignment::inherit) {
      result = attrAlignment;
    }
  }
  const TextAlignment styleAlignment{getInlineStyleAlignment(elem)};
  if (styleAlignment != TextAlignment::inherit) {
    result = styleAlignment;
  }
  return result == TextAlignment::inherit ? inherited : result;
}

auto hasTextContent(const XMLNode* parent) -> bool {
  for (const XMLNode* child{parent->FirstChild()}; child != nullptr;
       child = child->NextSibling()) {
    if (const XMLText* text = child->ToText()) {
      const std::string_view value{text->Value()};
      if (std::ranges::any_of(
              value, [](char ch) -> bool { return !isHTMLWhitespace(ch); })) {
        return true;
      }
    }
    if (child->ToElement() != nullptr && hasTextContent(child)) {
      return true;
    }
  }
  return false;
}

auto isBlockElement(std::string_view name) -> bool {
  return name == "p" || name == "li" || name == "pre" || name == "table"
         || name == "caption" || name == "hr" || name == "h1" || name == "h2"
         || name == "h3" || name == "h4" || name == "h5" || name == "h6"
         || name == "div" || name == "section" || name == "article"
         || name == "aside" || name == "main" || name == "header"
         || name == "footer" || name == "blockquote" || name == "center";
}

auto handlesAlignment(std::string_view name) -> bool {
  return isBlockElement(name) && name != "caption";
}

auto hasAlignmentBlockDescendant(const XMLNode* parent) -> bool {
  for (const XMLElement* child{parent->FirstChildElement()}; child != nullptr;
       child = child->NextSiblingElement()) {
    if (handlesAlignment(child->Name()) || hasAlignmentBlockDescendant(child)) {
      return true;
    }
  }
  return false;
}

auto isAlignmentContainer(std::string_view name) -> bool {
  return name == "section" || name == "article" || name == "aside"
         || name == "main" || name == "header" || name == "footer"
         || name == "blockquote" || name == "center";
}

auto appendAlignmentBegin(std::string& out, TextAlignment alignment) -> void {
  if (alignment == TextAlignment::center) {
    out += centerAlignBegin;
  }
  if (alignment == TextAlignment::right) {
    out += rightAlignBegin;
  }
}

auto appendAlignmentEnd(std::string& out, TextAlignment alignment) -> void {
  if (alignment == TextAlignment::center) {
    out += centerAlignEnd;
  }
  if (alignment == TextAlignment::right) {
    out += rightAlignEnd;
  }
}

auto isHeading(std::string_view name) -> bool {
  return name == "h1" || name == "h2" || name == "h3" || name == "h4"
         || name == "h5" || name == "h6";
}

struct TextStyle {
  bool bold{};
  bool italic{};
  bool underline{};
  std::string_view foreground;
};

auto appendStyleTransition(std::string& out, const TextStyle& current,
                           const TextStyle& next) -> void {
  if (current.bold != next.bold) {
    out += esc + (next.bold ? bold : resetBold);
  }
  if (current.italic != next.italic) {
    out += esc + (next.italic ? italic : resetItalic);
  }
  if (current.underline != next.underline) {
    out += esc + (next.underline ? underline : resetUnderline);
  }
  if (current.foreground != next.foreground) {
    if (!current.foreground.empty()) {
      out += esc + resetFG;
    }
    if (!next.foreground.empty()) {
      out += esc;
      out += next.foreground;
    }
  }
}

auto getTableCellSeparator(const TextStyle& style) -> std::string {
  TextStyle separatorStyle{style};
  separatorStyle.bold = false;
  separatorStyle.italic = false;
  separatorStyle.underline = false;
  separatorStyle.foreground = cyanFG;

  std::string separator{};
  appendStyleTransition(separator, style, separatorStyle);
  separator += " | ";
  appendStyleTransition(separator, separatorStyle, style);
  return separator;
}

auto parseContentElemImpl(const XMLElement* parent, std::string& out,
                          const fs::path& chapterAbs,
                          TextAlignment inheritedAlignment, TextStyle style)
    -> void;
} // namespace

auto getOPFRel(const fs::path& epubRootAbs) -> fs::path {
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

auto getMetadata(const XMLDocument& opf) -> const XMLElement* {
  return opf.FirstChildElement("package")->FirstChildElement("metadata");
}

auto getTitle(const XMLElement* metadata) -> std::string {
  return metadata->FirstChildElement("dc:title")->GetText();
}

auto getAuthor(const XMLElement* metadata) -> std::string {
  return metadata->FirstChildElement("dc:creator")->GetText();
}

auto getHrefFromID(const XMLElement* manifest, std::string_view id) -> const
    char* {
  for (const XMLElement* item{manifest->FirstChildElement("item")};
       item != nullptr; item = item->NextSiblingElement("item")) {
    if (item->Attribute("id") == id) {
      return item->Attribute("href");
    }
  }
  throw std::runtime_error{"element matching provided `<spine>` ID not "
                           "found in `<manifest>`"};
}

auto getSpine(const XMLDocument& opf) -> std::vector<fs::path> {
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
       itemref != nullptr; itemref = itemref->NextSiblingElement("itemref")) {

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

auto collectNavPoints(const XMLElement* parent, TocData& tocData,
                      const std::string& prefix) -> void {
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

auto getTOC(const fs::path& tocAbs) -> TocData {
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
auto parseContentElemImpl(const XMLElement* parent, std::string& out,
                          const fs::path& chapterAbs,
                          TextAlignment inheritedAlignment, TextStyle style)
    -> void {
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
      if (isBlock) {
        out += "\n\n";
      }

      if (name == "b" || name == "strong") {
        TextStyle childStyle{style};
        childStyle.bold = true;
        appendStyleTransition(out, style, childStyle);
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment,
                             childStyle);
        appendStyleTransition(out, childStyle, style);
      }
      else if (name == "i" || name == "em") {
        TextStyle childStyle{style};
        childStyle.italic = true;
        appendStyleTransition(out, style, childStyle);
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment,
                             childStyle);
        appendStyleTransition(out, childStyle, style);
      }
      else if (name == "code") {
        TextStyle childStyle{style};
        childStyle.foreground = greenFG;
        appendStyleTransition(out, style, childStyle);
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment,
                             childStyle);
        appendStyleTransition(out, childStyle, style);
      }
      else if (name == "br") {
        out += '\n';
      }
      else if (name == "hr") {
        TextStyle childStyle{style};
        childStyle.bold = true;
        childStyle.italic = false;
        childStyle.underline = false;
        childStyle.foreground = {};
        out += centerAlignBegin;
        appendStyleTransition(out, style, childStyle);
        out += "***";
        appendStyleTransition(out, childStyle, style);
        out += centerAlignEnd;
      }
      else if (isHeading(name)) {
        TextStyle childStyle{style};
        childStyle.bold = name == "h1" || name == "h2";
        childStyle.italic = name == "h3" || name == "h4";
        childStyle.underline = name == "h1" || name == "h3";
        childStyle.foreground = yellowFG;
        out += centerAlignBegin;
        appendStyleTransition(out, style, childStyle);
        parseContentElemImpl(childElem, out, chapterAbs, TextAlignment::center,
                             childStyle);
        appendStyleTransition(out, childStyle, style);
        out += centerAlignEnd;
      }
      else if (name == "p" || name == "li" || name == "div" || name == "pre"
               || name == "table") {
        TextStyle childStyle{style};
        if (name == "pre") {
          childStyle.foreground = greenFG;
        }
        const bool markAlignment{childAlignment != TextAlignment::left
                                 && hasTextContent(childElem)
                                 && !hasAlignmentBlockDescendant(childElem)};
        if (markAlignment) {
          appendAlignmentBegin(out, childAlignment);
        }
        appendStyleTransition(out, style, childStyle);
        const std::size_t contentBegin{out.size()};
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment,
                             childStyle);
        if (name == "pre") {
          std::size_t styleEnd{out.size()};
          while (styleEnd > contentBegin
                 && isHTMLWhitespace(out[styleEnd - 1])) {
            --styleEnd;
          }
          std::string styleTransition{};
          appendStyleTransition(styleTransition, childStyle, style);
          out.insert(styleEnd, styleTransition);
        }
        else {
          appendStyleTransition(out, childStyle, style);
        }
        if (markAlignment) {
          appendAlignmentEnd(out, childAlignment);
        }
      }
      else if (name == "tr") {
        out += '\n';
        const std::size_t contentBegin{out.size()};
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment, style);
        const std::string separator{getTableCellSeparator(style)};
        if (out.size() >= contentBegin + separator.size()
            && out.ends_with(separator)) {
          out.resize(out.size() - separator.size());
        }
      }
      else if (name == "td" || name == "th") {
        if (!childElem->NoChildren()) {
          TextStyle childStyle{style};
          if (name == "th") {
            childStyle.bold = true;
          }
          appendStyleTransition(out, style, childStyle);
          parseContentElemImpl(childElem, out, chapterAbs, childAlignment,
                               childStyle);
          appendStyleTransition(out, childStyle, style);
          out += getTableCellSeparator(style);
        }
      }
      else if (isAlignmentContainer(name)
               && childAlignment != TextAlignment::left
               && hasTextContent(childElem)
               && !hasAlignmentBlockDescendant(childElem)) {
        appendAlignmentBegin(out, childAlignment);
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment, style);
        appendAlignmentEnd(out, childAlignment);
      }
      else if (name == "image" || name == "img") {
        const std::string imgAttributeName{(name == "image") ? "xlink:href"
                                                             : "src"};
        std::string imgPathAbs{chapterAbs.parent_path()
                               / childElem->Attribute(imgAttributeName.data())};
        decodePercentEncoding(imgPathAbs);

        const std::size_t imageBegin{out.size()};
        try {
          out += "\n\n";
          displayImg(imgPathAbs, out);
          out += "\n\n";
        }
        catch (const std::runtime_error& e) {
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
      }
      else {
        parseContentElemImpl(childElem, out, chapterAbs, childAlignment, style);
      }

      if (isBlock) {
        out += "\n\n";
      }
    }
  }
}
} // namespace

auto parseContentElem(const XMLElement* parent, std::string& out,
                      const fs::path& chapterAbs) -> void {
  parseContentElemImpl(parent, out, chapterAbs,
                       getElementAlignment(parent, TextAlignment::left), {});
}

auto parseChapter(const fs::path& chapterAbs, std::string& out) -> void {
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
    }
    else if (untrimmed.substr(firstContent, nonBreakingSpace.size())
             == nonBreakingSpace) {
      firstContent += nonBreakingSpace.size();
    }
    else {
      break;
    }
  }

  out.erase(chapterBegin, firstContent - chapterBegin);

  while (out.size() > chapterBegin) {
    if (out.back() == '\n' || out.back() == ' ' || out.back() == '\t') {
      out.pop_back();
    }
    else if (out.size() - chapterBegin >= nonBreakingSpace.size()
             && out.ends_with(nonBreakingSpace)) {
      out.resize(out.size() - nonBreakingSpace.size());
    }
    else {
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

auto dumpEpub(const fs::path& epubRootAbs, std::string& out) -> void {
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

auto expandEllipsesAndTabs(std::string& str) -> void {
  findAndReplaceAll(str, "…", "...");
  findAndReplaceAll(str, "\t", "    ");
}
