#include <string>
#include <string_view>
#include "tui.hpp"

void findAndReplaceAll(std::string& str, std::string_view target, 
                       std::string_view replacement) {
    std::size_t pos = str.find(target);
    while (pos != std::string::npos) {
        str.replace(pos, target.size(), replacement);
        pos = str.find(target, pos + replacement.size());
    }
}
