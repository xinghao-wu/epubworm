#include <filesystem>
#include <stdexcept>
#include "tinyxml2.hpp"
#include "epub_parser.hpp"

namespace fs = std::filesystem;
using namespace tinyxml2;

fs::path getOPFRel(const fs::path& epubRootAbs) {
    const fs::path containerAbs {epubRootAbs / "META-INF/container.xml"};

    XMLDocument container {};
    container.LoadFile(containerAbs.c_str());

    if (container.Error()) {
        throw std::runtime_error{container.ErrorStr()};
    }

    return container.FirstChildElement("container")
                   ->FirstChildElement("rootfiles")
                   ->FirstChildElement("rootfile")
                   ->Attribute("full-path");
}

const XMLElement* getMetadata(const XMLDocument& opf) {
    return opf.FirstChildElement("package")->FirstChildElement("metadata");
}
