#pragma once

#include <string>
#include <string_view>

// in `str`, replace all occurences of `target` with `replacement`
void findAndReplaceAll(std::string& str, std::string_view target, 
                       std::string_view replacement);
