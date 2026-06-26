#include <filesystem>

namespace fs = std::filesystem;

namespace test {
    // assumes executable is ran with the working directory being build/
    extern const fs::path projectRoot;
    extern const fs::path mysteriesRoot;
    extern const fs::path parasiteRoot;
    extern const fs::path spiceWolfRoot;
    extern const fs::path zuttomoRoot;
}
