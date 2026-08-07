#pragma once

#include <filesystem>
#include <string>

struct ConfOpts {
    int lineLength{55};
};

// Extracts zipped archive to `destinationAbs`, creating directories as needed.
// Throws `std::runtime_error` for problematic `archiveAbs`.
// Throws `std::filesystem::filesystem_error` for problematic `destinationAbs`.
void unzip(const std::filesystem::path& archiveAbs,
           const std::filesystem::path& destinationAbs);

// Initialize a config xml file at `mncConfAbs`,
// writing the declaration and root elem `<conf>`.
void initConf(const std::filesystem::path& mncConfAbs);

// Initialize a library xml file `mncLibraryAbs`,
// writing the declaration and root elem `<library>`.
void initLibrary(const std::filesystem::path& mncLibraryAbs);

// Parse a config xml file and return the found options.
// If an option is not found within the file, a default value version of the
// option is written to the file.
// Throws `std::runtime_error` on failure to load `mncConfAbs`, failure to
// find root element `<conf>`, and on encountering invalid config options.
[[nodiscard]] ConfOpts readMncConf(const std::filesystem::path& mncConfAbs);

// Generates a SHA-256 hash of `fileAbs` via `shasum` and returns it truncated
// to 128 bits (32 hex characters).
// Throws `std::invalid_argument`, `std::system_error`, or `std::runtime_error`
// from `execute()` on failure to run `shasum`.
[[nodiscard]] std::string
getTruncatedSHA256Sum(const std::filesystem::path& fileAbs);
