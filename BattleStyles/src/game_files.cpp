// Game file reading through the loader's library - see game_files.h.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include "game_files.h"
#include "xv2_files.h"

namespace {

// The 256-byte limit of a cheap head read: a compressed archive file keeps
// its first 0x100 bytes raw (CRILAYLA prefix, XV2ModLoader Lib\src\game_files.cpp).
const size_t kMaxHeadBytes = 0x100;

// The library's functions, looked up once.
const XV2Files& Files()
{
    static XV2Files files = [] {
        XV2Files found;
        found.Load();
        return found;
    }();
    return files;
}

}  // namespace

bool GameFilesAvailable()
{
    const XV2Files& files = Files();
    return files.ReadGameFile && files.FindGameFile && files.ReadGameFilePart;
}

bool ReadGameFile(const std::string& path, std::vector<uint8_t>& out, std::string& source)
{
    if (!GameFilesAvailable()) return false;
    char from[MAX_PATH] = {};
    long long size = Files().FindGameFile(path.c_str(), from, sizeof(from));
    if (size < 0) return false;
    out.resize((size_t)size);
    if (Files().ReadGameFile(path.c_str(), out.data(), out.size()) != size) return false;
    source = from;
    return true;
}

bool GameFileExists(const std::string& path)
{
    return GameFilesAvailable() && Files().FindGameFile(path.c_str(), nullptr, 0) >= 0;
}

bool ReadGameFileEnds(const std::string& path, size_t headSize,
                      const std::function<uint64_t(const std::vector<uint8_t>& head, uint64_t fileSize)>& tailFrom,
                      std::vector<uint8_t>& head, std::vector<uint8_t>& tail)
{
    if (!GameFilesAvailable()) return false;
    long long fileSize = Files().FindGameFile(path.c_str(), nullptr, 0);
    if (fileSize < 0) return false;
    head.resize(std::min<size_t>({headSize, kMaxHeadBytes, (size_t)fileSize}));
    long long got = Files().ReadGameFilePart(path.c_str(), 0, head.data(), head.size());
    if (got < 0) return false;
    head.resize((size_t)got);
    uint64_t from = std::min<uint64_t>(tailFrom(head, (uint64_t)fileSize), (uint64_t)fileSize);
    tail.resize((size_t)((uint64_t)fileSize - from));
    if (tail.empty()) return true;
    got = Files().ReadGameFilePart(path.c_str(), from, tail.data(), tail.size());
    if (got < 0) return false;
    tail.resize((size_t)got);
    return true;
}

bool UseGameFilesLibrary(const std::wstring& libraryPath, const std::wstring& gameDir)
{
    // altered search path: lua54.dll is found next to the library (the flag
    // needs '\' separators, per LoadLibraryExW's documentation)
    std::wstring library = libraryPath;
    std::replace(library.begin(), library.end(), L'/', L'\\');
    if (!LoadLibraryExW(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)) return false;
    return GameFilesAvailable() && Files().SetGameFolder && Files().SetGameFolder(gameDir.c_str());
}
