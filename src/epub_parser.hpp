#pragma once

#include <filesystem>
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
