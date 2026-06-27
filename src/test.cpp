#include <filesystem>
#include <cassert>
#include <stdexcept>
#include <iostream>
#include "data_management.hpp"
#include "test.hpp"

namespace fs = std::filesystem;

namespace test {
    // assumes executable is ran with the working directory being build/
    const fs::path projectRootAbs {fs::current_path().parent_path()};

    const fs::path epubsAbs {projectRootAbs / "test_epubs"};
    const fs::path mysteriesRootAbs {epubsAbs / "lord_of_mysteries_unzipped"};
    const fs::path parasiteRootAbs {epubsAbs / "parasite_in_love_unzipped"};
    const fs::path spiceWolfRootAbs {epubsAbs / "spice_and_wolf_vol_1_unzipped"};
    const fs::path zuttomoRootAbs {epubsAbs / "zuttomo_vol_1_unzipped"};

    const fs::path xdgDirsAbs {projectRootAbs / ".testing_xdg_dirs"};
    const fs::path cacheAbs {xdgDirsAbs / ".cache"};
    const fs::path configAbs {xdgDirsAbs / ".config"};
    const fs::path shareAbs {xdgDirsAbs / "share"};

    void unzip() {
        const fs::path mysteriesZippedAbs {epubsAbs / "lord_of_mysteries.epub"};
        const fs::path parasiteZippedAbs {epubsAbs / "parasite_in_love.epub"};
        const fs::path spiceWolfZippedAbs {
            epubsAbs / "spice_and_wolf_vol_1.epub"};
        const fs::path zuttomoZippedAbs {epubsAbs / "zuttomo_vol_1.epub"};

        try {
            ::unzip("/problematic archive path", shareAbs);
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} == "bad zip");
        }
        try {
            ::unzip(parasiteZippedAbs, "/problematic destination path");
            assert(false);
        }
        catch (const fs::filesystem_error& e) {
            assert(std::string_view{e.what()} == 
                "filesystem error: "
                "cannot create directories: "
                "Permission denied [/problematic destination path/META-INF]");
        }
        std::cout << "test::unzip() failure cases passed\n";

        ::unzip(mysteriesZippedAbs, shareAbs / "lord_of_mysteries_unzipped");
        ::unzip(parasiteZippedAbs, shareAbs / "parasite_in_love_unzipped");
        ::unzip(spiceWolfZippedAbs, shareAbs / "spice_and_wolf_vol_1_unzipped");
        ::unzip(zuttomoZippedAbs, shareAbs / "zuttomo_vol_1_unzipped");
        std::cout << "test::unzip() success cases did not error, "
                     "check testing share dir to verify correct result\n";
    }
}
