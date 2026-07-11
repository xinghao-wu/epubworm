#pragma once

#include <filesystem>
#include <vector>
#include <utility>
#include "tinyxml2.hpp"

using namespace tinyxml2;
namespace fs = std::filesystem;
using TocData = std::vector<std::pair<std::string, fs::path>>;

// returns .opf file's path relative to `epubRootAbs`;
// throws `std::runtime_error` if epub's container.xml failed to be parsed;
// null pointer dereference occurs
// if file path is not at the expected location in container.xml
fs::path getOPFRel(const fs::path& epubRootAbs);

const XMLElement* getMetadata(const XMLDocument& opf);

std::string getTitle(const XMLElement* metadata);

std::string getAuthor(const XMLElement* metadata);

// searches `manifest` for element matching `id`, and returns corresponding href;
// throws `std::runtime_error` if no element matching id is found
const char* getHrefFromID(const XMLElement* manifest, std::string_view id);

// returns relative paths of the epub's xml files listed in <spine>;
// paths are relative to the root of the epub file;
// table of contents is the first element, rest follow in order of appearance;
// skips any elements with the attribute linear="no"
std::vector<fs::path> getSpine(const XMLDocument& opf);

// helper function for getTOC()
void collectNavPoints(const XMLElement* parent, TocData& tocData,
                      const std::string& prefix = "");

// returns a vector of pairs containing info about the ToC's navigation points;
// only works on toc.ncx files, not nav.xhtml;
// order of nav point pairs in vector is the same as their order in ToC;
// first element of pair is the nav point's name,
// second element is its file's path relative to the epub's root;
// nested nav points' name prefixed with four spaces for each level of nesting;
// throws `std::runtime_error` if unable to load `tocAbs`
TocData getTOC(const fs::path& tocAbs);
