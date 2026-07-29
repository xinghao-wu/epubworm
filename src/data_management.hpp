#pragma once

#include <filesystem>

namespace fs = std::filesystem;

// extracts zipped archive to `destinationAbs`, creating directories as needed;
// throws `std::runtime_error` for problematic `archiveAbs`;
// throws `fs::filesystem_error` for problematic `destinationAbs`
void unzip(const fs::path& archiveAbs, const fs::path& destinationAbs);

// initialize a config xml file, writing the declaration and root elem `<conf>`
void initConf(const fs::path& mncConfAbs);
