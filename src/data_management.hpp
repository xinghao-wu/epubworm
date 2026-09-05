#pragma once

#include "tinyxml2.hpp"
#include "tui.hpp"
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

struct ConfOpts {
    int lineLength{55};
};

struct EpubInfo {
    std::string id;
    std::string title;
    std::string author;
};

enum class LibraryUpdateError {
    alreadyInLibrary,
    notFoundOrAmbiguous,
};

// Extracts zipped archive to `destinationAbs`, creating directories as needed.
// Throws `std::runtime_error` for problematic `archiveAbs`.
// Throws `std::filesystem::filesystem_error` for problematic `destinationAbs`.
void unzip(const std::filesystem::path& archiveAbs,
           const std::filesystem::path& destinationAbs);

// Initialize a config xml file at `configFileAbs`,
// writing the declaration and root elem `<conf>`.
void initConf(const std::filesystem::path& configFileAbs);

// Initialize a library xml file `libraryFileAbs`,
// writing the declaration and root elem `<library>`.
void initLibrary(const std::filesystem::path& libraryFileAbs);

// Parse a config xml file and return the found options.
// If an option is not found within the file, a default value version of the
// option is written to the file.
// Throws `std::runtime_error` on failure to load `configFileAbs`, failure to
// find root element `<conf>`, and on encountering invalid config options.
[[nodiscard]] ConfOpts readConfig(const std::filesystem::path& configFileAbs);

// Set `<line-length>`'s `chars` attribute in the config file at
// `configFileAbs`.
// Creates the element if it is missing and preserves other config options.
// Throws `std::invalid_argument` if `chars` is not positive and
// `std::runtime_error` on load/save failure or a missing `<conf>` root.
void setConfigLineLength(const std::filesystem::path& configFileAbs,
                         int chars);

// Generates a SHA-256 hash of `fileAbs` via `shasum` on macOS and `sha256sum`
// otherwise, then returns it truncated to 128 bits (32 hex characters).
// Throws `std::invalid_argument`, `std::system_error`, or `std::runtime_error`
// from `execute()` on failure to run the hashing command.
[[nodiscard]] std::string
getTruncatedSHA256Sum(const std::filesystem::path& fileAbs);

// Reads an extracted epub's title and author and returns them with `id`.
// Throws `std::runtime_error` if its container or OPF cannot be loaded.
[[nodiscard]] EpubInfo getEpubInfo(std::string_view id,
                                   const std::filesystem::path& shareAbs);

// Returns pointer to the `<epub>` child of `libraryRoot` whose `id` attribute
// starts with `idPrefix`, or nullptr if there is no match or the prefix is
// ambiguous (matches more than one epub).
[[nodiscard]] tinyxml2::XMLElement*
findEpubById(tinyxml2::XMLElement* libraryRoot, std::string_view idPrefix);

// Queries an `<epub>` element for its id and `EpubProg`.
// `chapterAbs` is built as an absolute path:
// `shareAbs/epubworm/extracted_epubs/<id>/<opened-chapter>`.
// If `opened-chapter` is absent or empty, `chapterAbs` is left empty
// (so `displayEpub()` takes it as the first chapter).
// Throws `std::runtime_error` if the `<epub>` element is missing its `id`
// attribute, or if `chapter-progress` is missing or not a valid double.
[[nodiscard]] std::pair<std::string, EpubProg>
queryEpubElem(const tinyxml2::XMLElement* epub,
              const std::filesystem::path& shareAbs);

// Writes `prog` into an `<epub>` element's `opened-chapter` and
// `chapter-progress` attributes.
// `opened-chapter` is stored relative to
// `shareAbs/epubworm/extracted_epubs/<id>/` (the `<id>` is read from the
// element); an empty `chapterAbs` writes `""`.
// Throws `std::runtime_error` if the element is missing its `id` attribute.
void writeProgress(tinyxml2::XMLElement* epub, const EpubProg& prog,
                   const std::filesystem::path& shareAbs);

// Sets `<last-read>`'s `id` attribute to `id`.
// Throws `std::runtime_error` if `<library>` or `<last-read>` is missing.
void setLastRead(tinyxml2::XMLDocument& libraryDoc, std::string_view id);

// Returns the `id` of the last read epub from `<last-read>`.
// Throws `std::runtime_error` if `<library>` or `<last-read>` is missing,
// or if `<last-read>` is missing its `id` attribute.
[[nodiscard]] std::string getLastRead(const tinyxml2::XMLDocument& libraryDoc);

// Adds `zippedEpubAbs` to the library at `shareAbs/epubworm`.
// Computes the id hash, checks if already present, and if not, extracts the
// epub to `shareAbs/epubworm/extracted_epubs/<id>/`, adds an `<epub>` entry to
// `shareAbs/epubworm/library.xml`.
// Returns the added epub's info on success, or `alreadyInLibrary` if its ID is
// already present.
// Throws `std::runtime_error` on library load/save failure or a missing
// `<library>` element.
[[nodiscard]] std::expected<EpubInfo, LibraryUpdateError>
addToLibrary(const std::filesystem::path& zippedEpubAbs,
             const std::filesystem::path& shareAbs);

// Removes the epub whose `id` starts with `idPrefix` from the library at
// `shareAbs/epubworm`. Deletes the extracted epub at
// `shareAbs/epubworm/extracted_epubs/<full-id>/`, removes the `<epub>` entry
// from `shareAbs/epubworm/library.xml`, and resets `<last-read>` to a blank id
// if it referenced the removed epub.
// Returns the removed epub's info on success, or `notFoundOrAmbiguous` if no
// epub matches `idPrefix` or the prefix is ambiguous.
// Throws `std::runtime_error` on library load/save failure and on missing
// `<library>` or `<last-read>` elements.
[[nodiscard]] std::expected<EpubInfo, LibraryUpdateError>
deleteFromLibrary(std::string_view idPrefix,
                  const std::filesystem::path& shareAbs);

// Displays the epub whose `id` starts with `idPrefix` from the library at
// `shareAbs/epubworm`, using `desiredMaxLen` as the visual line length, and
// persists the exit progress back into `shareAbs/epubworm/library.xml`.
// Returns false if no epub matches `idPrefix` or the prefix is ambiguous;
// returns true on success.
// Throws `std::runtime_error` on library load/save failure, missing
// `<library>` or `<last-read>` elements, and from
// `displayEpub`/`queryEpubElem` on malformed data or bad initial progress.
bool readEpubInLibrary(std::string_view idPrefix,
                       const std::filesystem::path& shareAbs,
                       int desiredMaxLen);
