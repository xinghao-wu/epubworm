#include <filesystem>
#include "miniz_cpp.hpp"
#include "tinyxml2.hpp"
#include "data_management.hpp"

using namespace tinyxml2;
namespace fs = std::filesystem;

void unzip(const fs::path& archiveAbs, const fs::path& destinationAbs) {
    miniz_cpp::zip_file archive {archiveAbs.string()};
    archive.extractall(destinationAbs.string());
}

void initConf(const fs::path& mncConfAbs) {
    XMLDocument mncConf {};
    mncConf.InsertFirstChild(mncConf.NewDeclaration());
    mncConf.InsertEndChild(mncConf.NewElement("conf"));

    fs::create_directories(mncConfAbs.parent_path());
    if (mncConf.SaveFile(mncConfAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{std::string{"error saving conf file: "}
                + XMLDocument::ErrorIDToName(mncConf.ErrorID())};
    }
}

void initLibrary(const fs::path& mncLibraryAbs) {
    XMLDocument mncLibrary {};
    mncLibrary.InsertFirstChild(mncLibrary.NewDeclaration());
    mncLibrary.InsertEndChild(mncLibrary.NewElement("library"));

    fs::create_directories(mncLibraryAbs.parent_path());
    if (mncLibrary.SaveFile(mncLibraryAbs.c_str()) != XML_SUCCESS) {
        throw std::runtime_error{std::string{"error saving library file: "}
                + XMLDocument::ErrorIDToName(mncLibrary.ErrorID())};
    }
}
