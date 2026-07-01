#pragma once

#include <string>
#include <string_view>
#include <filesystem>

namespace fs = std::filesystem;

// in `str`, replace all occurences of `target` with `replacement`
constexpr void findAndReplaceAll(std::string& str, std::string_view target, 
                                 std::string_view replacement);

constexpr void wrapForTmuxPassthrough(std::string& str);

constexpr std::string getGraphicsEscCode(const fs::path& tempDataFileAbs,
                                         int channels, int xPixels,
                                         int yPixels, int rows, int cols);

void loadImg(const fs::path& imgAbs, int rows, int cols);

void displayLoadedImg(int rows, int cols);
