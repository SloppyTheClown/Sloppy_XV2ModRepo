#pragma once
#include <cstdint>
#include <string>
#include <vector>

// XV2 Mods Installer keeps a zip per installed x2m (data\InstallData\<GUID>.x2d); its
// x2m.xml holds the mod's name. Reading that is all this does.

// Unpacks one entry of a zip (stored or deflated). False when missing or damaged.
bool ReadZipEntry(const std::vector<uint8_t> &zip, const char *name, std::vector<uint8_t> &out);

// The value of <tag value="..." /> in an x2m.xml, decoded. Empty when absent.
std::wstring X2mXmlValue(const std::vector<uint8_t> &xml, const char *tag);

// The GUID from an X2M_SKILL.ini (GUID = "..."). Empty when absent or not GUID characters,
// so the result is safe to build a path from.
std::string X2mIniGuid(const std::vector<uint8_t> &ini);
