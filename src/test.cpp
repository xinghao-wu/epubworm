#include <filesystem>
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <string>
#include "tinyxml2.hpp"
#include "data_management.hpp"
#include "epub_parser.hpp"
#include "tui.hpp"
#include "test.hpp"

namespace fs = std::filesystem;
using namespace tinyxml2;

namespace test {
    // assumes executable is ran with the working directory being build/
    const fs::path projectRootAbs {fs::current_path().parent_path()};

    const fs::path epubsAbs {projectRootAbs / "test_epubs"};
    const fs::path mysteriesRootAbs 
            {epubsAbs / "lord_of_mysteries_vol_1_unzipped"};
    const fs::path parasiteRootAbs {epubsAbs / "parasite_in_love_unzipped"};
    const fs::path spiceWolfRootAbs {epubsAbs / "spice_and_wolf_vol_1_unzipped"};
    const fs::path zuttomoRootAbs {epubsAbs / "zuttomo_vol_1_unzipped"};

    const fs::path xdgDirsAbs {projectRootAbs / ".testing_xdg_dirs"};
    const fs::path cacheAbs {xdgDirsAbs / ".cache"};
    const fs::path configAbs {xdgDirsAbs / ".config"};
    const fs::path shareAbs {xdgDirsAbs / "share"};

    void unzip() {
        const fs::path mysteriesZippedAbs 
                {epubsAbs / "lord_of_mysteries_vol_1.epub"};
        const fs::path parasiteZippedAbs {epubsAbs / "parasite_in_love.epub"};
        const fs::path spiceWolfZippedAbs 
                {epubsAbs / "spice_and_wolf_vol_1.epub"};
        const fs::path zuttomoZippedAbs {epubsAbs / "zuttomo_vol_1.epub"};

        try {
            ::unzip("/bad archive path", shareAbs);
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} == "bad zip");
        }
        try {
            ::unzip(parasiteZippedAbs, "/bad destination path");
            assert(false);
        }
        catch (const fs::filesystem_error& e) {
            assert(std::string_view{e.what()} 
                   == "filesystem error: cannot create directories: "
                      "Permission denied [/bad destination path/META-INF]");
        }
        std::cout << "test::unzip() failure cases passed\n";

        ::unzip(mysteriesZippedAbs, 
                shareAbs / "lord_of_mysteries_vol_1_unzipped");
        ::unzip(parasiteZippedAbs, shareAbs / "parasite_in_love_unzipped");
        ::unzip(spiceWolfZippedAbs, shareAbs / "spice_and_wolf_vol_1_unzipped");
        ::unzip(zuttomoZippedAbs, shareAbs / "zuttomo_vol_1_unzipped");
        std::cout << "test::unzip() success cases did not error, "
                     "check testing share dir to verify correct result\n";
    }

    void getOPFRel() {
        try {
            ::getOPFRel("/bad epub root");
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} 
                   == "Error=XML_ERROR_FILE_NOT_FOUND "
                      "ErrorID=3 (0x3) Line number=0: "
                      "filename=/bad epub root/META-INF/container.xml");
        }
        std::cout << "test::getOPFRel() failure cases passed\n";

        assert(::getOPFRel(mysteriesRootAbs) == "content.opf");
        assert(::getOPFRel(parasiteRootAbs) == "OEBPS/content.opf");
        assert(::getOPFRel(spiceWolfRootAbs) == "content.opf");
        assert(::getOPFRel(zuttomoRootAbs) == "content.opf");
        std::cout << "test::getOPFRel() success cases passed\n";
    }

    void getMetadata() {
        XMLDocument mysteriesOPF {};
        mysteriesOPF.LoadFile((mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs))
                              .c_str());
        XMLDocument parasiteOPF {};
        parasiteOPF.LoadFile((parasiteRootAbs / ::getOPFRel(parasiteRootAbs))
                             .c_str());

        const XMLElement* mysteriesMetadata {::getMetadata(mysteriesOPF)};
        const XMLElement* parasiteMetadata {::getMetadata(parasiteOPF)};

        std::cout << "test::getMetadata() no failure cases\n";

        assert(std::string_view
               {mysteriesMetadata->FirstChildElement("dc:language")->GetText()}
               == "en");
        assert(std::string_view
               {parasiteMetadata->FirstChildElement("dc:publisher")->GetText()}
               == "ASCII Media Works");
        std::cout << "test::getMetadata() success cases passed\n";
    }

    void getTitle() {
        XMLDocument mysteriesOPF {};
        mysteriesOPF.LoadFile((mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs))
                              .c_str());
        XMLDocument spiceWolfOPF {};
        spiceWolfOPF.LoadFile((spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs))
                              .c_str());

        const XMLElement* mysteriesMetadata {::getMetadata(mysteriesOPF)};
        const XMLElement* spiceWolfMetadata {::getMetadata(spiceWolfOPF)};

        std::cout << "test::getTitle() no failure cases\n";

        assert(::getTitle(mysteriesMetadata) 
               == "Lord of Mysteries Volume 1: Clown");
        assert(::getTitle(spiceWolfMetadata) 
               == "Spice and Wolf, Vol. 1");
        std::cout << "test::getTitle() success cases passed\n";
    }

    void getAuthor() {
        XMLDocument mysteriesOPF {};
        mysteriesOPF.LoadFile((mysteriesRootAbs / ::getOPFRel(mysteriesRootAbs))
                              .c_str());
        XMLDocument spiceWolfOPF {};
        spiceWolfOPF.LoadFile((spiceWolfRootAbs / ::getOPFRel(spiceWolfRootAbs))
                              .c_str());

        const XMLElement* mysteriesMetadata {::getMetadata(mysteriesOPF)};
        const XMLElement* spiceWolfMetadata {::getMetadata(spiceWolfOPF)};

        std::cout << "test::getAuthor() no failure cases\n";

        assert(::getAuthor(mysteriesMetadata) 
               == "Cuttlefish That Loves Diving (爱潜水的乌贼)");
        assert(::getAuthor(spiceWolfMetadata) == "Isuna Hasekura");
        std::cout << "test::getAuthor() success cases passed\n";
    }

    void findAndReplaceAll() {
        std::cout << "test::findAndReplaceAll() no failure cases\n";

        std::string hasTarget {"target, target on the wall,"};
        ::findAndReplaceAll(hasTarget, "target", "mirror");
        assert(hasTarget == "mirror, mirror on the wall,");

        std::string noTarget {"am I the fairest of them all?"};
        ::findAndReplaceAll(noTarget, "target", "mirror");
        assert(noTarget == "am I the fairest of them all?");

        std::cout << "test::findAndReplaceAll() success cases passed\n";
    }

    void wrapForTmuxPassthrough() {
        std::cout << "test::wrapForTmuxPassthrough() no failure cases\n";

        std::string notEmpty {"\033]1337;SetProfile=NewProfileName\007"};
        ::wrapForTmuxPassthrough(notEmpty);
        assert(notEmpty 
               == "\033Ptmux;\033\033]1337;SetProfile=NewProfileName\007\033\\");

        std::string empty {""};
        ::wrapForTmuxPassthrough(empty);
        assert(empty == "\033Ptmux;\033\\");

        std::cout << "test::wrapForTmuxPassthrough() success cases passed\n";
    }

    void displayImg() {
        try {
            ::displayImg("/bad img path", 7, 7);
            assert(false);
        }
        catch (const std::runtime_error& e) {
            assert(std::string_view{e.what()} == "Unable to open file");
        }
        std::cout << "test::displayImg() failure cases passed\n";

        ::displayImg(mysteriesRootAbs / "images/Tarot Club - V01B - Justice.jpg",
                     35, 80);
        ::displayImg(parasiteRootAbs / "OEBPS/Images/cover.jpg", 35, 80);
        ::displayImg(parasiteRootAbs / "OEBPS/Images/ascii.png", 10, 40);
        ::displayImg(spiceWolfRootAbs / "OEBPS/images/Art_P6.jpg", 35, 80);
        ::displayImg(zuttomoRootAbs / "images/image1.jpeg", 20, 40);
        ::displayImg(zuttomoRootAbs / "images/image3.png", 25, 80);

        std::cout << "test::displayImg() success cases did not error, "
                     "check stdout for expected images\n";
    }
}
