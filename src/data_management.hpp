#pragma once

#include <filesystem>

namespace fs = std::filesystem;

// extracts zipped `archive` to `destination`, creating directories as needed;
// throws `std::runtime_error` for problematic `archive`;
// throws `fs::filesystem_error` for problematic `destination`
void unzip(const fs::path& archive, const fs::path& destination);
