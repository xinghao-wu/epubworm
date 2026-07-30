#pragma once

#include <filesystem>

namespace fs = std::filesystem;

struct ConfOpts {
    int lineLength {55};
};

// extracts zipped archive to `destinationAbs`, creating directories as needed;
// throws `std::runtime_error` for problematic `archiveAbs`;
// throws `fs::filesystem_error` for problematic `destinationAbs`
void unzip(const fs::path& archiveAbs, const fs::path& destinationAbs);

// initialize a config xml file at `mncConfAbs`,
// writing the declaration and root elem `<conf>`
void initConf(const fs::path& mncConfAbs);

// initialize a library xml file `mncLibraryAbs`,
// writing the declaration and root elem `<library>`
void initLibrary(const fs::path& mncLibraryAbs);

// parse a config xml file and return the found options;
// if an option is not found within the file, a default value version of the
// option is written to the file;
// throws `std::runtime_error` on failure to load `mncConfAbs`, failure to
// find root element `<conf>`, and on encountering invalid config options
ConfOpts readMncConf(const fs::path& mncConfAbs);
