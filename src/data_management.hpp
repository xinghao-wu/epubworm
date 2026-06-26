#include <filesystem>

namespace fs = std::filesystem;

// extracts zipped `archive` to `destination`, creating directories as needed
void unzip(const fs::path& archive, const fs::path& destination);
