#pragma once

#include "tinyxml2/tinyxml2.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Minimal CSS support: only the properties Epubworm can render (bold, italic,
// underline, and text alignment) are extracted, and only simple selectors
// (`tag`, `.class`, `tag.class`, `.class1.class2`, and comma separated lists
// of those) are matched. Rules with any other selector, and all @-rules
// (including their nested blocks), are ignored.

enum class CSSAlignment : std::uint8_t {
  left,
  center,
  right,
};

template <typename T>
struct CSSProperty {
  std::optional<T> value{};
  bool important{};
};

struct CSSDeclarations {
  CSSProperty<bool> bold{};
  CSSProperty<bool> italic{};
  CSSProperty<bool> underline{};
  CSSProperty<CSSAlignment> alignment{};
};

struct CSSRule {
  // Lowercase tag name, empty if selector doesn't restrict tag.
  std::string tag{};
  std::vector<std::string> classes{};
  CSSDeclarations declarations{};
};

struct Stylesheet {
  // In source order, multiple stylesheets are appended in document order.
  std::vector<CSSRule> rules{};
};

// Overlay `overlay` onto `base` following the cascade: a set property in
// `overlay` replaces the one in `base` unless only `base`'s is `!important`.
auto mergeCSSDeclarations(CSSDeclarations& base, const CSSDeclarations& overlay)
    -> void;

// Parse a declaration block's contents (e.g. a `style` attribute's value),
// such as `"font-weight: bold; text-align: center !important"`.
[[nodiscard]] auto parseCSSDeclarations(std::string_view declarations)
    -> CSSDeclarations;

// Parse stylesheet text, appending its supported rules to `out`.
auto parseStylesheet(std::string_view css, Stylesheet& out) -> void;

// Resolve all stylesheet rules matching `elem` together with its inline
// `style` attribute into the element's final declarations. Inherited values
// are not included.
[[nodiscard]] auto resolveCSS(const Stylesheet& stylesheet,
                              const tinyxml2::XMLElement* elem)
    -> CSSDeclarations;
