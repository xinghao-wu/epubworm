#include <filesystem>
#include "miniz_cpp.hpp"
#include "data_management.hpp"

namespace fs = std::filesystem;

void unzip(const fs::path& archive, const fs::path& destination) {
    miniz_cpp::zip_file loadedArchive {archive.string()};
    loadedArchive.extractall(destination.string());
}
