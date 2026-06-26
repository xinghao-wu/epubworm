#include <filesystem>
#include "test.hpp"

namespace fs = std::filesystem;

namespace test {
    extern const fs::path projectRoot {fs::current_path().parent_path()};
    extern const fs::path mysteriesRoot {
        projectRoot / "test_epubs/lord_of_mysteries_unzipped"};
    extern const fs::path parasiteRoot {
        projectRoot / "test_epubs/parasite_in_love_unzipped"};
    extern const fs::path spiceWolfRoot {
        projectRoot / "test_epubs/spice_and_wolf_vol_1_unzipped"};
    extern const fs::path zuttomoRoot {
        projectRoot / "test_epubs/zuttomo_vol_1_unzipped"};
    extern const fs::path relContainerXML {"META-INF/container.xml"};
}
