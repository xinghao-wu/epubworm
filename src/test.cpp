#include <filesystem>
#include "data_management.hpp"
#include "test.hpp"

namespace fs = std::filesystem;

namespace test {
    // assumes executable is ran with the working directory being build/
    const fs::path projectRoot {fs::current_path().parent_path()};

    const fs::path epubs {projectRoot / "test_epubs"};
    const fs::path mysteriesRoot {epubs / "lord_of_mysteries_unzipped"};
    const fs::path parasiteRoot {epubs / "parasite_in_love_unzipped"};
    const fs::path spiceWolfRoot {epubs / "spice_and_wolf_vol_1_unzipped"};
    const fs::path zuttomoRoot {epubs / "zuttomo_vol_1_unzipped"};

    const fs::path xdgDirs {projectRoot / ".testing_xdg_dirs"};
    const fs::path cache {xdgDirs / ".cache"};
    const fs::path config {xdgDirs / ".config"};
    const fs::path share {xdgDirs / "local/share"};

    void unzip() {
        const fs::path mysteriesZipped {epubs / "lord_of_mysteries.epub"};
        const fs::path parasiteZipped {epubs / "parasite_in_love.epub"};
        const fs::path spiceWolfZipped {epubs / "spice_and_wolf_vol_1.epub"};
        const fs::path zuttomoZipped {epubs / "zuttomo_vol_1.epub"};
        ::unzip(mysteriesZipped, share / "lord_of_mysteries_unzipped");
        ::unzip(parasiteZipped, share / "parasite_in_love_unzipped");
        ::unzip(spiceWolfZipped, share / "spice_and_wolf_vol_1_unzipped");
        ::unzip(zuttomoZipped, share / "zuttomo_vol_1_unzipped");
    }
}
