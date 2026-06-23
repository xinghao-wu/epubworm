#include <filesystem>
#include "test.hpp"

namespace fs = std::filesystem;

namespace test {
    extern const fs::path projectRoot {fs::current_path().parent_path()};
    extern const fs::path superCuteMeRoot {
        projectRoot / "test_epubs/date_this_super_cute_me_vol_1_unzipped"};
    extern const fs::path parasiteRoot {
        projectRoot / "test_epubs/parasite_in_love_unzipped"};
    extern const fs::path zuttomoRoot {
        projectRoot / "test_epubs/zuttomo_vol_1_unzipped"};
    extern const fs::path spiceAndWolfRoot {
        projectRoot / "test_epubs/spice_and_wolf_vol_1_unzipped"};
    extern const fs::path relContainerXML {"META-INF/container.xml"};
}
