#pragma once

#include <filesystem>

struct ConfOpts {
    int lineLength{55};
};

// extracts zipped archive to `destinationAbs`, creating directories as needed;
// throws `std::runtime_error` for problematic `archiveAbs`;
// throws `std::filesystem::filesystem_error` for problematic `destinationAbs`
void unzip(const std::filesystem::path& archiveAbs,
           const std::filesystem::path& destinationAbs);

// initialize a config xml file at `mncConfAbs`,
// writing the declaration and root elem `<conf>`
void initConf(const std::filesystem::path& mncConfAbs);

// initialize a library xml file `mncLibraryAbs`,
// writing the declaration and root elem `<library>`
void initLibrary(const std::filesystem::path& mncLibraryAbs);

// parse a config xml file and return the found options;
// if an option is not found within the file, a default value version of the
// option is written to the file;
// throws `std::runtime_error` on failure to load `mncConfAbs`, failure to
// find root element `<conf>`, and on encountering invalid config options
ConfOpts readMncConf(const std::filesystem::path& mncConfAbs);
