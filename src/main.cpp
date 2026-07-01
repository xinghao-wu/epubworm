#include "tui.hpp"

int main() {
    loadImg("/home/p1ea5ur3/projects/monocle/test_epubs/parasite_in_love_unzipped/OEBPS/Images/cover.jpg", 40, 80);
    displayLoadedImg(40, 80);
    loadImg("/home/p1ea5ur3/projects/monocle/test_epubs/parasite_in_love_unzipped/OEBPS/Images/ascii.png", 40, 80);
    displayLoadedImg(40, 80);
    return 0;
}
