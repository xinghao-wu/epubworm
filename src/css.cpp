#include "css.hpp"
#include "tinyxml2/tinyxml2.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace tinyxml2;

namespace {
constexpr auto isCSSWhitespace(char ch) -> bool {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f';
}

auto trim(std::string_view str) -> std::string_view {
  while (!str.empty() && isCSSWhitespace(str.front())) {
    str.remove_prefix(1);
  }
  while (!str.empty() && isCSSWhitespace(str.back())) {
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

auto isIdentChar(char ch) -> bool {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
         || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'
         || static_cast<unsigned char>(ch) >= 0x80;
}

// Find first of `chars` at or after `pos` that isn't inside a quoted string.
auto findUnquoted(std::string_view str, std::size_t pos, std::string_view chars)
    -> std::size_t {
  char quote{'\0'};
  for (; pos < str.size(); ++pos) {
    const char ch{str[pos]};
    if (ch == '\\') {
      ++pos;
    }
    else if (quote != '\0') {
      if (ch == quote) {
        quote = '\0';
      }
    }
    else if (ch == '"' || ch == '\'') {
      quote = ch;
    }
    else if (chars.contains(ch)) {
      return pos;
    }
  }
  return std::string_view::npos;
}

// `open` is the index of a `{`. Returns index of its matching `}`,
// or `npos` if the block is unterminated.
auto findBlockEnd(std::string_view str, std::size_t open) -> std::size_t {
  std::size_t depth{0};
  std::size_t pos{open};
  while ((pos = findUnquoted(str, pos, "{}")) != std::string_view::npos) {
    if (str[pos] == '{') {
      ++depth;
    }
    else if (--depth == 0) {
      return pos;
    }
    ++pos;
  }
  return std::string_view::npos;
}

auto stripComments(std::string_view css) -> std::string {
  std::string result{};
  result.reserve(css.size());
  std::size_t pos{0};
  while (pos < css.size()) {
    const std::size_t commentBegin{findUnquoted(css, pos, "/")};
    if (commentBegin == std::string_view::npos) {
      result += css.substr(pos);
      break;
    }
    if (css.substr(commentBegin).starts_with("/*")) {
      result += css.substr(pos, commentBegin - pos);
      result += ' ';
      const std::size_t commentEnd{css.find("*/", commentBegin + 2)};
      if (commentEnd == std::string_view::npos) {
        break;
      }
      pos = commentEnd + 2;
    }
    else {
      result += css.substr(pos, commentBegin + 1 - pos);
      pos = commentBegin + 1;
    }
  }
  return result;
}

auto parseFontWeight(std::string_view value) -> std::optional<bool> {
  if (value == "bold" || value == "bolder") {
    return true;
  }
  if (value == "normal" || value == "lighter") {
    return false;
  }
  int weight{};
  const auto [end, error]{
      std::from_chars(value.data(), value.data() + value.size(), weight)};
  if (error == std::errc{} && end == value.data() + value.size()) {
    return weight >= 600;
  }
  return std::nullopt;
}

auto parseFontStyle(std::string_view value) -> std::optional<bool> {
  if (value == "italic" || value.starts_with("oblique")) {
    return true;
  }
  if (value == "normal") {
    return false;
  }
  return std::nullopt;
}

auto parseTextDecoration(std::string_view value) -> std::optional<bool> {
  if (value == "none") {
    return false;
  }
  while (!value.empty()) {
    const std::size_t tokenEnd{value.find_first_of(" \t\n\r\f")};
    if (value.substr(0, tokenEnd) == "underline") {
      return true;
    }
    if (tokenEnd == std::string_view::npos) {
      break;
    }
    value = trim(value.substr(tokenEnd));
  }
  return std::nullopt;
}

auto parseTextAlign(std::string_view value) -> std::optional<CSSAlignment> {
  if (value == "center" || value == "-webkit-center") {
    return CSSAlignment::center;
  }
  if (value == "right" || value == "end" || value == "-webkit-right") {
    return CSSAlignment::right;
  }
  if (value == "left" || value == "start" || value == "justify"
      || value == "-webkit-left") {
    return CSSAlignment::left;
  }
  return std::nullopt;
}

template <typename T>
auto setProperty(CSSProperty<T>& property, std::optional<T> value,
                 bool important) -> void {
  if (!value.has_value()) {
    return;
  }
  if (important || !property.important) {
    property.value = value;
    property.important = important;
  }
}

auto parseSimpleSelector(std::string_view selector) -> std::optional<CSSRule> {
  selector = trim(selector);
  CSSRule rule{};
  std::size_t pos{0};
  while (pos < selector.size() && isIdentChar(selector[pos])) {
    ++pos;
  }
  rule.tag = lowerASCII(selector.substr(0, pos));
  while (pos < selector.size()) {
    if (selector[pos] != '.') {
      return std::nullopt;
    }
    const std::size_t classBegin{++pos};
    while (pos < selector.size() && isIdentChar(selector[pos])) {
      ++pos;
    }
    if (pos == classBegin) {
      return std::nullopt;
    }
    rule.classes.emplace_back(selector.substr(classBegin, pos - classBegin));
  }
  if (rule.tag.empty() && rule.classes.empty()) {
    return std::nullopt;
  }
  return rule;
}

auto addRules(std::string_view selectors, std::string_view body,
              Stylesheet& out) -> void {
  const CSSDeclarations declarations{parseCSSDeclarations(body)};
  while (true) {
    const std::size_t comma{findUnquoted(selectors, 0, ",")};
    if (std::optional<CSSRule> rule{
            parseSimpleSelector(selectors.substr(0, comma))}) {
      rule->declarations = declarations;
      out.rules.push_back(std::move(*rule));
    }
    if (comma == std::string_view::npos) {
      break;
    }
    selectors.remove_prefix(comma + 1);
  }
}

auto getClasses(const XMLElement* elem) -> std::vector<std::string_view> {
  std::vector<std::string_view> result{};
  const char* classAttr{elem->Attribute("class")};
  if (classAttr == nullptr) {
    return result;
  }
  std::string_view classes{classAttr};
  while (!classes.empty()) {
    classes = trim(classes);
    const std::size_t tokenEnd{classes.find_first_of(" \t\n\r\f")};
    if (!classes.empty()) {
      result.push_back(classes.substr(0, tokenEnd));
    }
    if (tokenEnd == std::string_view::npos) {
      break;
    }
    classes.remove_prefix(tokenEnd);
  }
  return result;
}

auto matches(const CSSRule& rule, std::string_view tag,
             const std::vector<std::string_view>& classes) -> bool {
  if (!rule.tag.empty() && rule.tag != tag) {
    return false;
  }
  return std::ranges::all_of(rule.classes, [&classes](const auto& cls) -> bool {
    return std::ranges::contains(classes, std::string_view{cls});
  });
}
} // namespace

auto mergeCSSDeclarations(CSSDeclarations& base, const CSSDeclarations& overlay)
    -> void {
  setProperty(base.bold, overlay.bold.value, overlay.bold.important);
  setProperty(base.italic, overlay.italic.value, overlay.italic.important);
  setProperty(base.underline, overlay.underline.value,
              overlay.underline.important);
  setProperty(base.alignment, overlay.alignment.value,
              overlay.alignment.important);
}

auto parseCSSDeclarations(std::string_view declarations) -> CSSDeclarations {
  CSSDeclarations result{};
  while (!declarations.empty()) {
    const std::size_t declarationEnd{findUnquoted(declarations, 0, ";")};
    const std::string_view declaration{declarations.substr(0, declarationEnd)};
    const std::size_t colon{declaration.find(':')};
    if (colon != std::string_view::npos) {
      const std::string property{
          lowerASCII(trim(declaration.substr(0, colon)))};
      std::string_view rawValue{trim(declaration.substr(colon + 1))};
      bool important{false};
      const std::size_t bang{rawValue.rfind('!')};
      if (bang != std::string_view::npos
          && lowerASCII(trim(rawValue.substr(bang + 1))) == "important") {
        important = true;
        rawValue = trim(rawValue.substr(0, bang));
      }
      const std::string value{lowerASCII(rawValue)};

      if (property == "font-weight") {
        setProperty(result.bold, parseFontWeight(value), important);
      }
      else if (property == "font-style") {
        setProperty(result.italic, parseFontStyle(value), important);
      }
      else if (property == "text-decoration"
               || property == "text-decoration-line") {
        setProperty(result.underline, parseTextDecoration(value), important);
      }
      else if (property == "text-align") {
        setProperty(result.alignment, parseTextAlign(value), important);
      }
    }

    if (declarationEnd == std::string_view::npos) {
      break;
    }
    declarations.remove_prefix(declarationEnd + 1);
  }
  return result;
}

auto parseStylesheet(std::string_view rawCSS, Stylesheet& out) -> void {
  const std::string stripped{stripComments(rawCSS)};
  const std::string_view css{stripped};
  std::size_t pos{0};
  while (pos < css.size()) {
    while (pos < css.size() && isCSSWhitespace(css[pos])) {
      ++pos;
    }
    const std::string_view rest{css.substr(pos)};
    if (rest.empty()) {
      break;
    }
    // HTML comment delimiters are allowed (and ignored) in `<style>` text.
    if (rest.starts_with("<!--")) {
      pos += 4;
      continue;
    }
    if (rest.starts_with("-->")) {
      pos += 3;
      continue;
    }

    if (rest.front() == '@') {
      const std::size_t end{findUnquoted(css, pos, ";{")};
      if (end == std::string_view::npos) {
        break;
      }
      if (css[end] == ';') {
        pos = end + 1;
        continue;
      }
      const std::size_t blockEnd{findBlockEnd(css, end)};
      if (blockEnd == std::string_view::npos) {
        break;
      }
      pos = blockEnd + 1;
      continue;
    }

    const std::size_t open{findUnquoted(css, pos, "{")};
    if (open == std::string_view::npos) {
      break;
    }
    const std::size_t close{findBlockEnd(css, open)};
    const std::string_view selectors{css.substr(pos, open - pos)};
    const std::string_view body{css.substr(
        open + 1, close == std::string_view::npos ? std::string_view::npos
                                                  : close - open - 1)};
    addRules(selectors, body, out);
    if (close == std::string_view::npos) {
      break;
    }
    pos = close + 1;
  }
}

auto resolveCSS(const Stylesheet& stylesheet, const XMLElement* elem)
    -> CSSDeclarations {
  CSSDeclarations result{};
  const char* styleAttr{elem->Attribute("style")};
  if (stylesheet.rules.empty() && styleAttr == nullptr) {
    return result;
  }

  const std::string tag{lowerASCII(elem->Name())};
  const std::vector<std::string_view> classes{getClasses(elem)};
  std::vector<const CSSRule*> matched{};
  for (const CSSRule& rule : stylesheet.rules) {
    if (matches(rule, tag, classes)) {
      matched.push_back(&rule);
    }
  }
  // Stable, so equal specificity rules keep source order.
  std::ranges::stable_sort(matched, {}, [](const CSSRule* rule) -> auto {
    // Number of classes, then whether a tag is present.
    return std::pair{rule->classes.size(), !rule->tag.empty()};
  });
  for (const CSSRule* rule : matched) {
    mergeCSSDeclarations(result, rule->declarations);
  }

  if (styleAttr != nullptr) {
    mergeCSSDeclarations(result, parseCSSDeclarations(styleAttr));
  }
  return result;
}
