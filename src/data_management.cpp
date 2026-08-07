#include "data_management.hpp"
#include "miniz_cpp.hpp"
#include "tinyxml2.hpp"
#include "tui.hpp"
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tinyxml2;
namespace fs = std::filesystem;

void unzip(const fs::path& archiveAbs, const fs::path& destinationAbs) {
    miniz_cpp::zip_file archive{archiveAbs.string()};
    archive.extractall(destinationAbs.string());
}

void initConf(const fs::path& mncConfAbs) {
    XMLDocument mncConf{};
    mncConf.InsertFirstChild(mncConf.NewDeclaration());
    mncConf.InsertEndChild(mncConf.NewElement("conf"));

    fs::create_directories(mncConfAbs.parent_path());
    if (mncConf.SaveFile(mncConfAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving conf file: "}
                + XMLDocument::ErrorIDToName(mncConf.ErrorID())};
    }
}

void initLibrary(const fs::path& mncLibraryAbs) {
    XMLDocument mncLibrary{};
    mncLibrary.InsertFirstChild(mncLibrary.NewDeclaration());
    mncLibrary.InsertEndChild(mncLibrary.NewElement("library"));

    fs::create_directories(mncLibraryAbs.parent_path());
    if (mncLibrary.SaveFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }
}

ConfOpts readMncConf(const fs::path& mncConfAbs) {
    XMLDocument mncConf{};
    mncConf.LoadFile(mncConfAbs.c_str());

    if (mncConf.Error()) {
        throw std::runtime_error{mncConf.ErrorStr()};
    }

    XMLElement* const rootElem{mncConf.FirstChildElement("conf")};
    if (rootElem == nullptr) {
        throw std::runtime_error{"root element <conf> missing in config file"};
    }

    // Write a default value for unfound options to support adding future
    // conf options without making users edit config file every time.
    if (rootElem->FirstChildElement("line-length") == nullptr) {
        rootElem->InsertEndChild(mncConf.NewElement("line-length"));
    }

    XMLElement* const lineLength{rootElem->FirstChildElement("line-length")};
    if (lineLength->Attribute("chars") == nullptr) {
        lineLength->SetAttribute("chars", 55);
    }

    int chars{0};
    lineLength->QueryIntAttribute("chars", &chars);
    if (chars <= 0) {
        throw std::runtime_error{"conf option line-length has invalid value"};
    }

    if (mncConf.SaveFile(mncConfAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{
                std::string{"error saving conf file: "}
                + XMLDocument::ErrorIDToName(mncConf.ErrorID())};
    }

    return {chars};
}

std::string getTruncatedSHA256Sum(const fs::path& fileAbs) {
    const std::string sha256{execute(std::vector<std::string>{
            "shasum", "-a", "256", fileAbs.string()})};
    return sha256.substr(0, 32); // 128 bits = 32 hex chars
}
