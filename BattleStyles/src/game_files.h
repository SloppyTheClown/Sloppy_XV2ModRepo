// Read-only access to one file of the game's data: loose <game>\data first
// (what XV2Patcher loads), then the CRIWARE .cpk archives in <game>\cpk -
// through the loader's library (XV2Files_ReadGameFile and friends,
// XV2ModLoader\sdk\xv2_files.h), which reads the archive tables once per
// session (about a second, on the first call that needs them).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// False when the loader's library is missing or older than its game file
// reading: every function below then finds nothing.
bool GameFilesAvailable();

// `path` relative to data, e.g. "chara/GOK/GOK.ean". False if found nowhere.
// `source` = "loose" or the .cpk's name.
bool ReadGameFile(const std::string& path, std::vector<uint8_t>& out, std::string& source);

// True if `path` is a loose file or in an archive; reads no file data.
bool GameFileExists(const std::string& path);

// Reads only the two ends of a file: `head` = its first `headSize` bytes
// (at most 0x100), then `tail` = everything from offset
// `tailFrom(head, fileSize)` to the end. A compressed archive file is
// decoded from its end backwards, so the middle is neither read nor
// decompressed (for names at the end of a big file, e.g. an EAN's).
bool ReadGameFileEnds(const std::string& path, size_t headSize,
                      const std::function<uint64_t(const std::vector<uint8_t>& head, uint64_t fileSize)>& tailFrom,
                      std::vector<uint8_t>& head, std::vector<uint8_t>& tail);

// Offline tests (outside the game): loads the loader's library from
// `libraryPath` and reads the game at `gameDir`. Call before anything else.
bool UseGameFilesLibrary(const std::wstring& libraryPath, const std::wstring& gameDir);
