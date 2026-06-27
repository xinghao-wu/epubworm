#include <filesystem>
#include "miniz_cpp.hpp"
#include "data_management.hpp"

namespace fs = std::filesystem;

void unzip(const fs::path& archiveAbs, const fs::path& destinationAbs) {
    miniz_cpp::zip_file archive {archiveAbs.string()};
    archive.extractall(destinationAbs.string());
}
