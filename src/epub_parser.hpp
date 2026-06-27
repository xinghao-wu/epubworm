#pragma once

#include <filesystem>
#include "tinyxml2.hpp"

namespace fs = std::filesystem;
using namespace tinyxml2;

// returns .opf file's path relative to `epubRootAbs`;
// throws `std::runtime_error` if epub's container.xml failed to be parsed
// or if the path is not at the expected location in container.xml
fs::path getOPFRel(const fs::path& epubRootAbs);
