#pragma once

#include "tinyxml2.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using TocData = std::vector<std::pair<std::string, std::filesystem::path>>;

// Note: for all functions that traverse xml,
// malformed or unexpected xml may cause either a null pointer to be returned,
// or a direct null pointer dereference inside the function. The rationale
// for omitting null pointer checks whose sole purpose is to stop a
// null pointer dereference is that there's rarely a difference between
// a null pointer dereference crash or an exception being thrown,
// since encountering problematic xml is usually a fatal error anyways.

// Returns .opf file's path relative to `epubRootAbs`.
// Throws `std::runtime_error` if epub's `container.xml` failed to be loaded.
[[nodiscard]] std::filesystem::path
getOPFRel(const std::filesystem::path& epubRootAbs);

[[nodiscard]] const tinyxml2::XMLElement*
getMetadata(const tinyxml2::XMLDocument& opf);

[[nodiscard]] std::string getTitle(const tinyxml2::XMLElement* metadata);

[[nodiscard]] std::string getAuthor(const tinyxml2::XMLElement* metadata);

// Searches `manifest` for child element with attribute of value `id`,
// returning relative file path found in that element's `href` attribute.
// Unless an exception is thrown, the caller can safely assume the return
// value points to a valid, null-terminated string containing a file path.
// Throws `std::runtime_error` if no element matching `id` is found.
[[nodiscard]] const char* getHrefFromID(const tinyxml2::XMLElement* manifest,
                                        std::string_view id);

// Returns relative paths of the epub's xml files listed in `<spine>`,
// paths are relative to opf file's parent directory.
// In returned vector, the table of contents is the first element,
// the rest follow in order of appearance.
// Skips any elements with the attribute `linear="no"` (`nav.xhtml` usually).
[[nodiscard]] std::vector<std::filesystem::path>
getSpine(const tinyxml2::XMLDocument& opf);

// Recursive helper function for `getTOC()`.
void collectNavPoints(const tinyxml2::XMLElement* parent, TocData& tocData,
                      const std::string& prefix = "");

// Returns a vector of pairs containing info about the TOC's navigation points.
// Only works on `toc.ncx` files (EPUB 2, but found in most EPUB 3 epubs),
// not `nav.xhtml` files (EPUB 3).
// Order of nav point pairs in vector is the same as their order in the TOC.
// First element of pair is the nav point's name,
// second element is its file's path relative to `toc.ncx`'s parent directory.
// Nested nav points' names prefixed with four spaces for each level of
// nesting. Throws `std::runtime_error` if unable to load `tocAbs`.
[[nodiscard]] TocData getTOC(const std::filesystem::path& tocAbs);

// Parse all text and elements contained within `parent` recursively,
// appending result to `out`.
// The elements `<em>`, `<i>`, `<strong>`, `<b>`, `<br/>`, `<h1>` to `<h6>`,
// `<p>`, `<li>`, `<image/>`, and `<img/>` will be handled,
// all other elements will be ignored and traversed through.
void parseContentElem(const tinyxml2::XMLElement* parent, std::string& out,
                      const std::filesystem::path& chapterAbs);

// Parse chapter content xhtml file using `parseContentElem()`,
// appending result to `out`.
// Chapter's extraneous leading and trailing newlines, spaces, and non-breaking
// spaces are deleted. A red foreground colored `"---\n"` is appended to
// chapter text.
void parseChapter(const std::filesystem::path& chapterAbs, std::string& out);

// Parse all chapters of epub, appending result to `out`.
// Mostly for testing purposes, getting the whole epub at once is inefficient.
void dumpEpub(const std::filesystem::path& epubRootAbs, std::string& out);

// In `str`, expand the ugly unicode ellipses (…) into three normal dots (...)
// and escaped tab characters `(\t)` into four spaces.
void expandEllipsesAndTabs(std::string& str);
