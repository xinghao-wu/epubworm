#pragma once

#include <filesystem>
#include <vector>
#include "tinyxml2.hpp"

namespace fs = std::filesystem;
using namespace tinyxml2;

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
