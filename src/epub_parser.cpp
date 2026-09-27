#include "epub_parser.hpp"
#include "css.hpp"
#include "percent_encoding_decode.hpp"
#include "tinyxml2/tinyxml2.hpp"
#include "tui.hpp"
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
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

auto toTextAlignment(CSSAlignment alignment) -> TextAlignment {
  switch (alignment) {
  case CSSAlignment::left:
    return TextAlignment::left;
  case CSSAlignment::center:
    return TextAlignment::center;
  case CSSAlignment::right:
    return TextAlignment::right;
  }
  return TextAlignment::inherit;
}

// Precedence, lowest to highest: semantic class name heuristics, `<center>`
// and legacy `align` attribute, then CSS (stylesheet rules and inline style,
// already cascaded in `css`).
auto getElementAlignment(const XMLElement* elem, TextAlignment inherited,
                         const CSSDeclarations& css) -> TextAlignment {
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
  if (css.alignment.value.has_value()) {
    result = toTextAlignment(*css.alignment.value);
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
  std::string_view foreground{};
  bool lockDescendantStyles{};
};

// CSS is applied after an element's built-in tag styling, so author styles
// override it, as in browsers. Locked (heading) styles ignore CSS.
auto applyCSSStyle(TextStyle style, const CSSDeclarations& css) -> TextStyle {
  if (style.lockDescendantStyles) {
    return style;
  }
  style.bold = css.bold.value.value_or(style.bold);
  style.italic = css.italic.value.value_or(style.italic);
  style.underline = css.underline.value.value_or(style.underline);
  return style;
}

auto appendStyleTransition(std::string& out, const TextStyle& current,
                           const TextStyle& next) -> void {
  if (current.lockDescendantStyles && next.lockDescendantStyles) {
    return;
  }
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

// Style `appendContent()`'s output with `inner`, transitioning from and back
// to `outer`. The opening transition is placed after the content's leading
// whitespace and the closing transition before its trailing whitespace, so
// style codes don't split up block separators and source indentation (which
// would stop them from being trimmed or collapsed). Content without anything
// visible gets no transitions.
auto appendStyled(std::string& out, const TextStyle& outer,
                  const TextStyle& inner,
                  const std::invocable auto& appendContent) -> void {
  const std::size_t contentBegin{out.size()};
  appendContent();
  std::string opening{};
  appendStyleTransition(opening, outer, inner);
  if (opening.empty()) {
    return;
  }

  std::size_t textBegin{contentBegin};
  while (textBegin < out.size() && isHTMLWhitespace(out[textBegin])) {
    ++textBegin;
  }
  std::size_t textEnd{out.size()};
  while (textEnd > textBegin && isHTMLWhitespace(out[textEnd - 1])) {
    --textEnd;
  }
  if (textBegin == textEnd) {
    return;
  }
  std::string closing{};
  appendStyleTransition(closing, inner, outer);
  out.insert(textEnd, closing);
  out.insert(textBegin, opening);
}

auto parseContentElemImpl(const XMLElement* parent, std::string& out,
                          const fs::path& chapterAbs,
                          const Stylesheet& stylesheet,
                          TextAlignment inheritedAlignment, TextStyle style,
                          bool insideTableCell) -> void;
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
  std::string result{};
  for (const XMLElement* creator{metadata->FirstChildElement("dc:creator")};
       creator != nullptr;
       creator = creator->NextSiblingElement("dc:creator")) {
    const char* name{creator->GetText()};
    if (name == nullptr || *name == '\0') {
      continue;
    }
    if (!result.empty()) {
      result += " & ";
    }
    result += name;
  }
  return result;
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

    const fs::path srcPathRel{srcFilePathRel};
    if (std::ranges::none_of(tocData,
                             [&srcPathRel](const auto& navPointData) -> bool {
                               return navPointData.second == srcPathRel;
                             })) {
      tocData.emplace_back(prefix + name, srcPathRel);
    }
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
                          const Stylesheet& stylesheet,
                          TextAlignment inheritedAlignment, TextStyle style,
                          bool insideTableCell) -> void {
  for (const XMLNode* childNode{parent->FirstChild()}; childNode != nullptr;
       childNode = childNode->NextSibling()) {
    if (const XMLText* childText = childNode->ToText()) {
      out += childText->Value();
    }
    if (const XMLElement* childElem = childNode->ToElement()) {
      const std::string_view name{childElem->Name()};
      const CSSDeclarations css{resolveCSS(stylesheet, childElem)};
      const TextAlignment childAlignment{
          getElementAlignment(childElem, inheritedAlignment, css)};
      const bool isBlock{isBlockElement(name)};
      if (isBlock && !insideTableCell) {
        out += "\n\n";
      }

      if (name == "b" || name == "strong") {
        TextStyle childStyle{style};
        childStyle.bold = true;
        childStyle = applyCSSStyle(childStyle, css);
        appendStyled(out, style, childStyle, [&]() -> void {
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
        });
      }
      else if (name == "i" || name == "em") {
        TextStyle childStyle{style};
        childStyle.italic = true;
        childStyle = applyCSSStyle(childStyle, css);
        appendStyled(out, style, childStyle, [&]() -> void {
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
        });
      }
      else if (name == "code") {
        TextStyle childStyle{style};
        childStyle.foreground = greenFG;
        childStyle = applyCSSStyle(childStyle, css);
        appendStyled(out, style, childStyle, [&]() -> void {
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
        });
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
        childStyle.foreground = magentaFG;
        childStyle.lockDescendantStyles = true;
        out += centerAlignBegin;
        appendStyleTransition(out, style, childStyle);
        parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                             TextAlignment::center, childStyle,
                             insideTableCell);
        appendStyleTransition(out, childStyle, style);
        out += centerAlignEnd;
      }
      else if (name == "p" || name == "li" || name == "div" || name == "pre"
               || name == "table") {
        TextStyle childStyle{style};
        if (name == "pre") {
          childStyle.foreground = greenFG;
        }
        childStyle = applyCSSStyle(childStyle, css);
        const bool markAlignment{childAlignment != TextAlignment::left
                                 && hasTextContent(childElem)
                                 && !hasAlignmentBlockDescendant(childElem)};
        if (markAlignment) {
          appendAlignmentBegin(out, childAlignment);
        }
        if (name == "pre") {
          appendStyleTransition(out, style, childStyle);
          const std::size_t contentBegin{out.size()};
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
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
          appendStyled(out, style, childStyle, [&]() -> void {
            parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                                 childAlignment, childStyle, insideTableCell);
          });
        }
        if (markAlignment) {
          appendAlignmentEnd(out, childAlignment);
        }
      }
      else if (name == "tr") {
        const TextStyle childStyle{applyCSSStyle(style, css)};
        out += '\n';
        appendStyled(out, style, childStyle, [&]() -> void {
          const std::size_t contentBegin{out.size()};
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
          const std::string separator{getTableCellSeparator(childStyle)};
          if (out.size() >= contentBegin + separator.size()
              && out.ends_with(separator)) {
            out.resize(out.size() - separator.size());
          }
        });
      }
      else if (name == "td" || name == "th") {
        if (!childElem->NoChildren()) {
          TextStyle childStyle{style};
          if (name == "th") {
            childStyle.bold = true;
          }
          childStyle = applyCSSStyle(childStyle, css);
          appendStyled(out, style, childStyle, [&]() -> void {
            parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                                 childAlignment, childStyle, true);
          });
          out += getTableCellSeparator(style);
        }
      }
      else if (isAlignmentContainer(name)
               && childAlignment != TextAlignment::left
               && hasTextContent(childElem)
               && !hasAlignmentBlockDescendant(childElem)) {
        const TextStyle childStyle{applyCSSStyle(style, css)};
        appendAlignmentBegin(out, childAlignment);
        appendStyled(out, style, childStyle, [&]() -> void {
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
        });
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
        const TextStyle childStyle{applyCSSStyle(style, css)};
        appendStyled(out, style, childStyle, [&]() -> void {
          parseContentElemImpl(childElem, out, chapterAbs, stylesheet,
                               childAlignment, childStyle, insideTableCell);
        });
      }

      if (isBlock && !insideTableCell) {
        out += "\n\n";
      }
    }
  }
}
} // namespace

auto parseContentElem(const XMLElement* parent, std::string& out,
                      const fs::path& chapterAbs, const Stylesheet& stylesheet)
    -> void {
  parseContentElemImpl(parent, out, chapterAbs, stylesheet,
                       getElementAlignment(parent, TextAlignment::left,
                                           resolveCSS(stylesheet, parent)),
                       {}, false);
}

namespace {
auto loadChapterStylesheet(const XMLElement* head, const fs::path& chapterAbs)
    -> Stylesheet {
  Stylesheet result{};
  if (head == nullptr) {
    return result;
  }
  for (const XMLElement* elem{head->FirstChildElement()}; elem != nullptr;
       elem = elem->NextSiblingElement()) {
    const std::string_view name{elem->Name()};
    if (name == "style") {
      if (const char* text{elem->GetText()}) {
        parseStylesheet(text, result);
      }
      continue;
    }
    if (name != "link") {
      continue;
    }
    const char* rel{elem->Attribute("rel")};
    const char* href{elem->Attribute("href")};
    if (rel == nullptr || href == nullptr) {
      continue;
    }
    bool isStylesheet{false};
    bool isAlternate{false};
    std::string_view relTokens{rel};
    while (!relTokens.empty()) {
      relTokens = trim(relTokens);
      const std::size_t tokenEnd{relTokens.find_first_of(" \t\n\r\f")};
      const std::string token{lowerASCII(relTokens.substr(0, tokenEnd))};
      isStylesheet = isStylesheet || token == "stylesheet";
      isAlternate = isAlternate || token == "alternate";
      if (tokenEnd == std::string_view::npos) {
        break;
      }
      relTokens.remove_prefix(tokenEnd);
    }
    if (!isStylesheet || isAlternate) {
      continue;
    }

    std::string hrefRel{href};
    if (hrefRel.contains('#')) {
      hrefRel.resize(hrefRel.find('#'));
    }
    decodePercentEncoding(hrefRel);
    // Missing or unreadable stylesheets are silently ignored,
    // like missing images.
    std::ifstream file{(chapterAbs.parent_path() / hrefRel).lexically_normal(),
                       std::ios::binary};
    if (!file) {
      continue;
    }
    const std::string css{std::istreambuf_iterator<char>{file},
                          std::istreambuf_iterator<char>{}};
    parseStylesheet(css, result);
  }
  return result;
}
} // namespace

auto parseChapter(const fs::path& chapterAbs, std::string& out) -> void {
  XMLDocument chapter{};
  chapter.LoadFile(chapterAbs.c_str());

  if (chapter.Error()) {
    throw std::runtime_error{chapter.ErrorStr()};
  }

  const XMLElement* const html{chapter.FirstChildElement("html")};
  const XMLElement* const body{html->FirstChildElement("body")};
  const Stylesheet stylesheet{
      loadChapterStylesheet(html->FirstChildElement("head"), chapterAbs)};

  const std::size_t chapterBegin{out.size()};
  parseContentElem(body, out, chapterAbs, stylesheet);

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
