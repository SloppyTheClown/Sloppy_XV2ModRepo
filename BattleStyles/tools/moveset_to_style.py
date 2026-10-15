"""Turns a moveset mod into a Battle Style installer.

Moveset mods usually overwrite a race's (or a roster character's) moveset:
chara/HUM/HUM.ean, HUM_PLAYER.bac/.bcm/.bdm, its effects and SE bank. This
tool reads such a mod and writes a new LB Mod Installer that adds the moveset
under new CMS codes instead, plus a style file
(data\\battle_styles\\styles\\<name>.xml) that Battle Styles lists as one style.
Nothing the game already has is replaced. Inputs:
  - an LB Mod Installer (.exe + .installinfo)
  - an x2m character (its moveset only; the x2m's CMS values, AI, effects
    and the SE bank its CSO entry names - the model is HUM's, unused)
  - loose files: "data" folders, or bare <CODE>.ean / <CODE>_PLAYER.bac ...
  - a .rar/.zip/.7z of any of these (unpacked to a temp folder), or a folder

    python moveset_to_style.py <mod folder | .installinfo | .exe | .x2m | .rar ...> [output root] [--no-install]

The mod ships it as tools\\moveset_to_style.exe (PyInstaller, build.bat
:converter), so players need no Python; same arguments.

Output: next to a dropped file (inside a dropped folder), "<the mod's
installer name, else "<name> Installer"> (Battle Styles Compatible).exe" (a
copy of the mod's own LB Mod Installer exe, else of tools\\lb_installer's)
and its .installinfo; that installer is then started unless --no-install.
The name is cleaned (clean_name). With an output root:
<output root>\\Battle Style - <name>\\<name> (Battle Style) Installer.exe.

What the generated installer adds (data-relative):
  chara/<NEW>/<NEW>.ean, .cam.ean, <NEW>_PLAYER.bac/.bcm/.bdm - each moveset variant
  vfx/chara/<NEW>/<NEW>.eepk + assets      - the variant's effects; assets the
                                            mod relied on from the race's own
                                            folder are copied from the game
  sound/SE/Battle/Chara/CAR_BTL_<NEW>_SE.acb - the variant's SE bank (found by
                                            name by Battle Styles: LB can't
                                            bind a CSO entry to an AutoCharaID)
  system/char_model_spec.cms entry (AutoCharaID 400..999), vfx/vfx_spec.ers
  entry (same id), battle_styles/styles/<name>.xml
  anything else the mod installs (e.g. skill/CMN projectile merges), unchanged

Variants with identical files share one code (HUM = SYM in most mods). A
race without its own variant gets the same sex's (Battle Styles,
StyleMenu_MovesetFor). Uninstall: run the generated installer again.

Requires the game (XV2_GAME or Steam's library) for the race's CMS values
and the effect files the mod didn't ship.
"""
import hashlib
import io
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib
import xml.etree.ElementTree as ET
from xml.sax.saxutils import quoteattr

# The tools folder: the script's own, or, in the shipped PyInstaller exe
# (tools\moveset_to_style.exe, build.bat :converter), the exe's - __file__
# then points into the exe's temporary unpack folder.
if getattr(sys, "frozen", False):
    HERE = os.path.dirname(os.path.abspath(sys.executable))
else:
    HERE = os.path.dirname(os.path.abspath(__file__))
WORKSPACE = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
# Appended to the source installer's name for the converted one; also how a
# re-run tells its own earlier output apart from the original mod.
OUTPUT_TAG = " (Battle Styles Compatible)"
# The cpk reader: a copy next to this file in the mod release
# (tools\xv2data\cpk.py, build.bat release), XV2DataEdit's in the workspace.
sys.path[:0] = [HERE, os.path.join(WORKSPACE, "Tools", "XV2DataEdit")]
from xv2data.cpk import CpkSet  # noqa: E402

# Created-character races, CMS ids 100..107, and the female ones (Battle
# Styles falls back to a same-sex variant for a race the mod doesn't cover).
RACES = ["HUM", "HUF", "SYM", "SYF", "NMC", "FRI", "MAM", "MAF"]
FEMALE = {"HUF", "SYF", "MAF"}

# CMS ids the generated entries may take (LB AutoCharaID): above every
# vanilla/DLC character (max 313 in 1.26.01) and clear of the low ids
# x2m characters get first.
CMS_ID_RANGE = (400, 999)
# New codes are "9" + two base-36 digits: no game character code starts with
# a digit, so they can't collide with the roster.
CODE_PREFIX = "9"
BASE36 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

# CMS binary layout (Xv2CoreLib CMS/Parser.cs): count at +8, table offset at
# +0xc, 0x54 bytes per entry; id +0, code +4 (4 bytes), I_08 int64, I_16
# int32, LoadCamDist/I_22/I_24/I_26 uint16 at +20..+26, I_28 int32 at +28,
# then absolute string offsets for the path fields below.
CMS_ENTRY_SIZE = 0x54
CMS_STRINGS = [("BCS", 32), ("EAN", 36), ("FCE_EAN", 44), ("FCE", 48), ("CAM_EAN", 56),
               ("BAC", 60), ("BCM", 64), ("BAI", 68), ("BDM", 80)]

# Moveset parts by file name inside chara/<CODE>/ (the CMS path getter adds
# "_PLAYER" to BAC/BCM/BDM) -> CMS field.
MOVESET_PARTS = {".ean": "EAN", ".cam.ean": "CAM_EAN", "_player.bac": "BAC", "_player.bcm": "BCM", "_player.bdm": "BDM"}
# Files an EEPK names as its assets (effect parts, textures, materials, ...).
EFFECT_ASSET = re.compile(rb"([\x21-\x7e][\x20-\x7e]{0,126}\.(?:emb|emm|emo|ema|emp|etr|ecf|emd|esk|scd|eff))\x00", re.I)
# The game files a moveset mod edits that the new codes replace.
DROPPED_MERGES = {"system/char_model_spec.cms", "system/chara_sound.cso", "vfx/vfx_spec.ers"}


def log(text=""):
    print(text, flush=True)


def warn(text):
    print("  WARNING: " + text, flush=True)


# ------------------------------------------------------------------ game ---
def find_game():
    """The game folder: %XV2_GAME%, the game this copy is installed in
    (<game>\\XV2ModLoader\\mods\\BattleStyles\\tools), or Steam's own library."""
    if os.environ.get("XV2_GAME"):
        return os.environ["XV2_GAME"]
    installed_in = os.path.normpath(os.path.join(HERE, "..", "..", "..", ".."))
    if os.path.isfile(os.path.join(installed_in, "bin", "DBXV2.exe")):
        return installed_in
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as key:
            steam = winreg.QueryValueEx(key, "SteamPath")[0].replace("/", "\\")
    except OSError:
        return None
    libraries = [steam]
    try:
        with open(os.path.join(steam, "steamapps", "libraryfolders.vdf"), encoding="utf-8") as vdf:
            libraries += [p.replace("\\\\", "\\") for p in re.findall(r'"path"\s+"([^"]+)"', vdf.read())]
    except OSError:
        pass
    for library in libraries:
        game = os.path.join(library, "steamapps", "common", "DB Xenoverse 2")
        if os.path.isfile(os.path.join(game, "bin", "DBXV2.exe")):
            return game
    return None


# The cpks store game files under "data/" (data/system/char_model_spec.cms);
# this tool's paths are data-relative like the loose folder's.
CPK_ROOT = "data/"


class Game:
    """Reads game files the way the game does: loose data first, then the cpks."""

    def __init__(self, folder):
        self.folder = folder
        self._cpks = None

    def cpks(self):
        if self._cpks is None:
            self._cpks = CpkSet(os.path.join(self.folder, "cpk"))
        return self._cpks

    def loose(self, path):
        return os.path.join(self.folder, "data", *path.split("/"))

    def read(self, path):
        loose = self.loose(path)
        if os.path.isfile(loose):
            with open(loose, "rb") as f:
                return f.read()
        try:
            return self.cpks().read(CPK_ROOT + path)
        except FileNotFoundError:
            return None

    def exists(self, path):
        return os.path.isfile(self.loose(path)) or self.cpks().find(CPK_ROOT + path) is not None

    def folder_files(self, folder):
        """lower-case name -> path of every file directly in data-relative `folder`."""
        folder = folder.strip("/") + "/"
        found = {}
        for key in self.cpks().list(CPK_ROOT + folder):
            rest = key[len(CPK_ROOT + folder):]
            if "/" not in rest:
                found[rest] = self.cpks().name(key)[len(CPK_ROOT):]
        loose = self.loose(folder)
        if os.path.isdir(loose):
            for name in os.listdir(loose):
                if os.path.isfile(os.path.join(loose, name)):
                    found[name.lower()] = folder + name
        return found


def read_cms(data):
    """code -> {"id", "I_08".."I_28", path fields} of every CMS entry."""
    count, table = struct.unpack_from("<iI", data, 8)
    entries = {}

    def string_at(offset):
        if offset <= 0 or offset >= len(data):
            return ""
        return data[offset:data.index(b"\0", offset)].decode("ascii", "replace")

    for i in range(count):
        at = table + i * CMS_ENTRY_SIZE
        chara_id = struct.unpack_from("<i", data, at)[0]
        code = data[at + 4:at + 8].split(b"\0")[0].decode("ascii", "replace").upper()
        entry = {"id": chara_id, "code": code,
                 "I_08": struct.unpack_from("<q", data, at + 8)[0],
                 "I_16": struct.unpack_from("<i", data, at + 16)[0],
                 "LoadCamDist": struct.unpack_from("<H", data, at + 20)[0],
                 "I_22": struct.unpack_from("<H", data, at + 22)[0],
                 "I_24": struct.unpack_from("<H", data, at + 24)[0],
                 "I_26": struct.unpack_from("<H", data, at + 26)[0],
                 "I_28": struct.unpack_from("<i", data, at + 28)[0]}
        for field, offset in CMS_STRINGS:
            entry[field] = string_at(struct.unpack_from("<i", data, at + offset)[0])
        entries[code] = entry
    return entries


# ------------------------------------------------------------- mod input ---
class ModFile:
    """One file the mod installs: where it goes and where its bytes come from."""

    def __init__(self, install_path, read, origin, merge_type=None, source_path=None):
        self.install_path = install_path  # data-relative, "/" separators
        self.read = read                  # () -> bytes
        self.origin = origin              # for the report
        self.merge_type = merge_type      # "XML"/"Binary" when LB merges it into a game file
        self.source_path = source_path    # the merge file inside data/ of the installinfo


def normalize(path):
    return path.replace("\\", "/").lstrip("/")


def lb_file_type(file_element):
    """LB Mod Installer's FilePath.GetFileType (InstallerXml.cs) for one <File>."""
    source_element = file_element.find("SourcePath")
    source = source_element.get("value", "") if source_element is not None else ""
    declared = file_element.get("Type", "Default")
    if declared == "Binding":
        return declared, source
    if declared == "Default":
        if source.endswith("/") or source.endswith("\\"):
            return "CopyDir", source
        return ("XML" if os.path.splitext(source)[1] == ".xml" else "CopyFile"), source
    return declared, source


def read_installinfo(path):
    """Every file an LB installer can install, all options included (default-
    selected options first, so they win when two options write one file)."""
    archive = zipfile.ZipFile(path)
    names = archive.namelist()
    if "data/battle_style/char_model_spec.cms.xml" in names:  # written by build() below
        sys.exit("This is already a Battle Style installer (made by this converter): "
                 "drop the original moveset mod instead.")
    root = ET.fromstring(archive.read("InstallerXml.xml"))
    info = {"name": (root.find("Name").get("value") if root.find("Name") is not None else "") or "",
            "version": root.find("Version").get("value") if root.find("Version") is not None else "1.0",
            "author": root.find("Author").get("value") if root.find("Author") is not None else "",
            "ui": root.find("UiOverrides"), "archive": archive}

    file_groups = []  # (origin, [File elements]) in priority order
    for step_number, step in enumerate(root.iter("InstallStep"), 1):
        options = step.findall("Option")
        if not options:  # Type "Message" steps (a text page) install nothing
            continue
        selected_element = step.find("SelectedOptions")
        default_text = selected_element.get("Default", "") if selected_element is not None else ""
        selected = [int(n) for n in re.findall(r"\d+", default_text) if int(n) < len(options)] or [0]
        order = selected + [n for n in range(len(options)) if n not in selected]
        for n in order:
            option = options[n]
            label = option.find("Message").get("value", "") if option.find("Message") is not None else str(n)
            file_groups.append((f"step {step_number} option \"{label}\"", option.findall("File")))
    files_to_install = root.find("FilesToInstall")
    if files_to_install is not None:
        file_groups.insert(0, ("FilesToInstall", files_to_install.findall("File")))

    files = []
    for origin, elements in file_groups:
        for element in elements:
            file_type, source = lb_file_type(element)
            install_element = element.find("InstallPath")
            install = normalize(install_element.get("value", "")) if install_element is not None else ""
            if file_type == "CopyDir":
                # Install.cs: JUNGLE3/<source> if it has files, else data/<source>
                for prefix in ("JUNGLE3/" + source, "data/" + source):
                    entries = [n for n in names if n.startswith(prefix) and not n.endswith("/")]
                    if entries:
                        for entry in entries:
                            files.append(ModFile(install.rstrip("/") + "/" + entry[len(prefix):].lstrip("/"),
                                                 lambda e=entry: archive.read(e), origin))
                        break
                else:
                    warn(f"{origin}: folder {source} is not in the installer")
            elif file_type == "CopyFile":
                entry = "data/" + source
                if entry in names:
                    files.append(ModFile(install, lambda e=entry: archive.read(e), origin))
                else:
                    warn(f"{origin}: {entry} is not in the installer")
            elif file_type in ("XML", "Binary"):
                files.append(ModFile(install, lambda e="data/" + source: archive.read(e), origin, file_type, source))
            else:
                warn(f"{origin}: {file_type} file {source} is not supported - left out")
    # JUNGLE1 / JUNGLE2: copied into data as they are (Install.cs ProcessJungle)
    for jungle in ("JUNGLE1/", "JUNGLE2/"):
        for entry in names:
            if entry.startswith(jungle) and not entry.endswith("/"):
                files.append(ModFile(entry[len(jungle):], lambda e=entry: archive.read(e), jungle.rstrip("/")))
    return info, files


def bare_file_install_path(name):
    """Where a moveset file found outside any "data" folder belongs, by its
    name alone ("MAF.ean" -> chara/MAF/MAF.ean; many race moveset mods are
    just these files), or None for anything else."""
    match = re.fullmatch(r"([A-Za-z0-9]{3})(\.cam\.ean|\.ean|_PLAYER\.(?:bac|bcm|bdm))", name, re.I)
    if match:
        return f"chara/{match.group(1).upper()}/{name}"
    if re.fullmatch(r"CAR_BTL_[A-Za-z0-9]{3}_SE\.(acb|awb)", name, re.I):
        return "sound/SE/Battle/Chara/" + name
    return None


def read_loose_folder(folder):
    """Loose-file mods: every "data" folder under `folder`, as if copied into
    the game, plus moveset files lying anywhere else (bare_file_install_path)."""
    files, ignored = [], 0
    for current, dirs, names in os.walk(folder):
        for d in list(dirs):
            if d.lower() == "data":
                data = os.path.join(current, d)
                for sub, _, data_names in os.walk(data):
                    for name in data_names:
                        full = os.path.join(sub, name)
                        files.append(ModFile(normalize(os.path.relpath(full, data)),
                                             lambda p=full: open(p, "rb").read(), os.path.relpath(data, folder)))
                dirs.remove(d)
        for name in names:
            if OUTPUT_TAG in name:  # an installer this converter wrote earlier
                continue
            install = bare_file_install_path(name)
            if install:
                full = os.path.join(current, name)
                files.append(ModFile(install, lambda p=full: open(p, "rb").read(),
                                     normalize(os.path.relpath(full, folder))))
            else:
                ignored += 1
    if ignored:
        log(f"  {ignored} other file(s) left out (not a moveset file or a data folder)")
    return files


# x2m character entry (eternity's CmsEntryXV, x2m.xml <Entry>) -> the CMS
# binary fields read_cms names (U_10 sits at +0x10 = I_16, ...; I_08 is
# always 0 - CmsFile.h).
X2M_CMS_VALUES = {"I_16": "U_10", "LoadCamDist": "LOAD_CAM_DIST", "I_22": "U_16", "I_24": "U_18",
                  "I_26": "U_1A", "I_28": "U_1C"}
X2M_CMS_PATHS = {"BCS": "CHARACTER", "EAN": "EAN", "FCE_EAN": "FCE_EAN", "FCE": "FCE", "CAM_EAN": "CAM_EAN",
                 "BAC": "BAC", "BCM": "BCM", "BAI": "AI", "BDM": "STR_50"}


def read_x2m_folder(folder):
    """An unpacked x2m (xv2ins mod): a new character's moveset from its
    character folder, or a replacer's data files from JUNGLE. Returns
    (info, files, x2m) - x2m holds what the character's own CMS/CSO/ERS
    entries said, since that character is not in the game."""
    root = ET.parse(os.path.join(folder, "x2m.xml")).getroot()

    def value(tag, element=root):
        found = element.find(tag)
        return found.get("value", "") if found is not None else ""

    kind = root.get("type", "")
    name = value("MOD_NAME") or value("CHARA_NAME_EN")
    info = {"name": name, "version": value("MOD_VERSION") or "1.0", "author": value("MOD_AUTHOR"),
            "ui": None, "archive": None}
    log(f"x2m ({kind}): {name}")
    if kind not in ("NEW_CHARACTER", "REPLACER"):
        sys.exit(f"This x2m is a {kind.replace('_', ' ').lower()} mod, not a character: it has no moveset")

    files, left_out = [], 0
    jungle = os.path.join(folder, "JUNGLE", "data")
    if os.path.isdir(jungle):
        for current, _, names in os.walk(jungle):
            for file_name in names:
                full = os.path.join(current, file_name)
                files.append(ModFile(normalize(os.path.relpath(full, jungle)),
                                     lambda p=full: open(p, "rb").read(), "x2m JUNGLE"))
    x2m = None
    if kind == "NEW_CHARACTER":
        code = value("ENTRY_NAME").upper()
        entry = root.find("Entry")
        cso = root.find("CsoEntry")
        if not code or entry is None:
            sys.exit("The x2m has no character entry")
        cms = {"id": None, "code": code, "I_08": 0}
        for field, tag in X2M_CMS_VALUES.items():
            cms[field] = int(value(tag, entry) or "0", 0)
        for field, tag in X2M_CMS_PATHS.items():
            cms[field] = value(tag, entry)
        x2m = {"code": code, "cms": cms,
               "se_bank": value("SE", cso) if cso is not None else "",
               "eepk": value("CHAR_EEPK"), "model": {}}
        character = os.path.join(folder, code)
        for file_name in sorted(os.listdir(character)) if os.path.isdir(character) else []:
            full = os.path.join(character, file_name)
            install = bare_file_install_path(file_name)
            if os.path.isfile(full) and install and install.lower().startswith(f"chara/{code.lower()}/"):
                files.append(ModFile(install, lambda p=full: open(p, "rb").read(), "x2m " + code))
            else:
                if os.path.isfile(full):
                    x2m["model"][file_name.lower()] = full  # x2m_prop_files may take a few
                left_out += 1
        if left_out:
            log(f"  {left_out} model/other file(s) of {code} left out (only the moveset is used)")
    return info, files, x2m


ARCHIVE_EXTENSIONS = (".rar", ".zip", ".7z", ".x2m")


def unpack_archive(path, into):
    """Unpacks a .rar/.zip/.7z/.x2m into `into`: zip-based ones with Python,
    the rest with Windows' own tar.exe (libarchive: rar, rar5, 7z; Windows 10
    1803+), then 7-Zip if it is installed."""
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as archive:
            archive.extractall(into)  # zipfile drops ".." and drive parts of entry names
        return
    windows = os.environ.get("SystemRoot", r"C:\Windows")
    tools = [[os.path.join(windows, "System32", "tar.exe"), "-xf", path, "-C", into]]
    for program_files in (os.environ.get("ProgramFiles", ""), os.environ.get("ProgramFiles(x86)", "")):
        tools.append([os.path.join(program_files, "7-Zip", "7z.exe"), "x", "-y", "-o" + into, path])
    errors = []
    for command in tools:
        if not os.path.isfile(command[0]):
            continue
        result = subprocess.run(command, capture_output=True, text=True, errors="replace")
        if result.returncode == 0 and any(files for _, _, files in os.walk(into)):
            return
        errors.append(f"{os.path.basename(command[0])}: {(result.stderr or result.stdout).strip()[:300]}")
    sys.exit(f"Could not unpack {os.path.basename(path)}" + (": " + "; ".join(errors) if errors else
             " (no tar.exe or 7-Zip found): unpack it and drop the folder instead"))


def unwrap(folder):
    """The folder that holds the mod: archives often wrap it in one folder
    of the same name."""
    while True:
        entries = os.listdir(folder)
        if len(entries) != 1 or not os.path.isdir(os.path.join(folder, entries[0])):
            return folder
        folder = os.path.join(folder, entries[0])


def find_installinfo(folder):
    """The mod's LB installer in `folder` or one level down (a mod folder
    with its installer in a sub folder), never one this converter wrote."""
    found = []
    for current, dirs, names in os.walk(folder):
        depth = os.path.relpath(current, folder).count(os.sep) + (current != folder)
        if depth >= 1:
            dirs[:] = []
        found += [(depth, os.path.join(current, n)) for n in sorted(names)
                  if n.lower().endswith(".installinfo") and OUTPUT_TAG not in n]
    if not found:
        return None
    found.sort()
    if len(found) > 1:
        warn(f"several installers here, using {os.path.basename(found[0][1])}: "
             + ", ".join(os.path.basename(path) for _, path in found))
    return found[0][1]


def is_lb_installer(path):
    try:
        with open(path, "rb") as f:
            return "LB Mod Installer".encode("utf-16-le") in f.read()
    except OSError:
        return False


# LB Mod Installer (LazyBone, MIT - lb_installer\LICENSE.txt), shipped with
# the converter for mods that bring no installer of their own (x2m, rar of
# loose files, data folders).
BUNDLED_LB_EXE = os.path.join(HERE, "lb_installer", "LB Mod Installer.exe")


def find_lb_exe(installinfo, folder):
    """The mod's own LB Mod Installer exe, else None."""
    if installinfo:
        exe = os.path.splitext(installinfo)[0] + ".exe"
        if os.path.isfile(exe):
            return exe
    for current, _, names in os.walk(folder):
        for name in sorted(names):
            if OUTPUT_TAG in name:  # an installer this converter wrote earlier
                continue
            if name.lower().endswith(".exe") and is_lb_installer(os.path.join(current, name)):
                return os.path.join(current, name)
    return None


# Race codes, the CaC tag and leftovers download sites put in file names.
NAME_JUNK_WORDS = {code.lower() for code in RACES} | {"cac", "cacs"}


def clean_name(raw):
    """A readable style name from a file/folder/mod name:
    "1729306997_Kick-Tempest-MAF" -> "Kick Tempest", "SYF-Muay-Thai" -> "Muay Thai",
    "Cool Moveset v1.2 (1)" -> "Cool Moveset". Names that are already clean
    ("Gun Fu Rush Moveset") stay as they are, so their style codes stay too."""
    name = raw.strip()
    name = re.sub(r"^\d{6,}[\s_\-]+", "", name)               # Xenoverse Mods upload id prefix
    name = re.sub(r"(-\d+){2,}$", "", name)                   # Nexus "-1234-1-0-1700000000" suffix
    name = re.sub(r"\s*\(\d+\)$", "", name)                    # Windows "(1)" duplicate download
    name = re.sub(r"\s+installer$", "", name, flags=re.I)
    if " " not in name:                                        # a slug: "Kick-Tempest-MAF", "super_hero"
        name = re.sub(r"[_\-]+", " ", name)
    else:
        name = name.replace("_", " ")
    words = [w for w in name.split() if w.lower().strip("[]()") not in NAME_JUNK_WORDS
             and not re.fullmatch(r"v\d+(\.\d+)*", w, re.I)]
    name = " ".join(words) if words else name
    name = re.sub(r"\s+", " ", name).strip(" -_.")
    if name and name == name.lower():
        name = " ".join(w[:1].upper() + w[1:] for w in name.split(" "))
    return name or raw.strip()


class ModSource:
    """What the user dropped, read: the files the mod installs and where the
    converted installer goes."""

    def __init__(self):
        self.info = None        # name/version/author/ui/archive (read_installinfo's dict)
        self.files = []
        self.installinfo = None
        self.folder = None      # the mod's (unpacked) folder: its own LB exe is looked for here
        self.x2m = None         # read_x2m_folder's character entries
        self.name = ""
        self.out_dir = None     # where the converted installer is written
        self.exe = None         # the LB Mod Installer exe to copy


def read_mod(target, scratch):
    """Reads an LB installer (.exe/.installinfo), an x2m, an archive or a
    folder of any of these or of loose files. Archives unpack into `scratch`."""
    source = ModSource()
    title = os.path.basename(os.path.normpath(target))
    lower = target.lower()
    if os.path.isfile(target):
        source.out_dir = os.path.dirname(target)
        if lower.endswith(".exe"):
            target = os.path.splitext(target)[0] + ".installinfo"
            lower = target.lower()
            if not os.path.isfile(target):
                sys.exit(f"No {os.path.basename(target)} next to the installer .exe: keep the mod's files together")
        if lower.endswith(".installinfo"):
            folder, installinfo = os.path.dirname(target), target
        elif lower.endswith(ARCHIVE_EXTENSIONS):
            log(f"Unpacking {os.path.basename(target)}")
            unpack_archive(target, scratch)
            title = os.path.splitext(title)[0]
            folder, installinfo = unwrap(scratch), None
        else:
            sys.exit(f"Not a moveset mod: {target}\n"
                     "Drop an LB Mod Installer .exe, an .x2m, a .rar/.zip/.7z or a mod folder.")
    else:
        folder, installinfo = target, None
        source.out_dir = target  # next to the mod's own installer, inside its folder

    if installinfo is None and os.path.isfile(os.path.join(folder, "x2m.xml")):
        source.info, source.files, source.x2m = read_x2m_folder(folder)
    else:
        installinfo = installinfo or find_installinfo(folder)
        if installinfo is None:
            x2ms = [os.path.join(current, n) for current, _, names in os.walk(folder)
                    for n in sorted(names) if n.lower().endswith(".x2m")]
            if x2ms:  # a folder or archive holding an x2m
                if len(x2ms) > 1:
                    warn("several x2m files here, using " + os.path.basename(x2ms[0]))
                inner = os.path.join(scratch, "_x2m")
                os.makedirs(inner, exist_ok=True)
                unpack_archive(x2ms[0], inner)
                source.info, source.files, source.x2m = read_x2m_folder(inner)
                folder = inner
    if installinfo:
        log(f"Reading {installinfo}")
        source.info, source.files = read_installinfo(installinfo)
    elif source.info is None:
        log(f"Reading loose files in {title}")
        source.info = {"name": "", "version": "1.0", "author": "", "ui": None, "archive": None}
        source.files = read_loose_folder(folder)
    source.installinfo, source.folder = installinfo, folder
    source.name = clean_name(source.info["name"] or title)
    source.exe = find_lb_exe(installinfo, folder) if not source.x2m else None
    return source


# -------------------------------------------------------------- classify ---
class Variant:
    """One race's (or character's) moveset as the mod ships it."""

    def __init__(self, source_code):
        self.source_code = source_code  # the CMS code the mod overwrites (HUM, GOK, ...)
        self.parts = {}                 # CMS field -> bytes
        self.effects = {}               # file name -> bytes (vfx/chara/<code>/)
        self.eepk_name = None           # the effect folder's <code>.eepk
        self.sound = {}                 # ".acb"/".awb" -> bytes
        self.eepk_fallback = None       # vfx-relative EEPK used when the mod ships none (x2m CHAR_EEPK)
        self.code = None                # the new CMS code

    def signature(self):
        digest = hashlib.sha1()
        for field in sorted(self.parts):
            digest.update(field.encode() + hashlib.sha1(self.parts[field]).digest())
        # the EEPK is named after the race (HUM.eepk, SYM.eepk): compared by content only
        labels = {("<eepk>" if name == self.eepk_name else name.lower()): name for name in self.effects}
        for label in sorted(labels):
            digest.update(label.encode() + hashlib.sha1(self.effects[labels[label]]).digest())
        for ext in sorted(self.sound):
            digest.update(ext.encode() + hashlib.sha1(self.sound[ext]).digest())
        return digest.hexdigest()


def classify(files):
    """Splits the mod's files into moveset variants, shared files and dropped ones."""
    chosen = {}  # lower install path -> ModFile (first wins)
    merges = []
    for f in files:
        if f.merge_type:
            if f.install_path.lower() in DROPPED_MERGES:
                log(f"  left out (new codes get their own entries): {f.install_path} merge ({f.origin})")
            else:
                merges.append(f)
            continue
        key = f.install_path.lower()
        if key in chosen:
            if chosen[key].read() != f.read():
                warn(f"{f.install_path}: {f.origin} differs from {chosen[key].origin} - kept the first")
            continue
        chosen[key] = f

    variants, shared = {}, []
    for key, f in chosen.items():
        parts = f.install_path.split("/")
        lower = [p.lower() for p in parts]
        if len(parts) == 3 and lower[0] == "chara":
            code = parts[1].upper()
            part = next((field for suffix, field in MOVESET_PARTS.items()
                         if lower[2] == code.lower() + suffix), None)
            if part:
                variants.setdefault(code, Variant(code)).parts[part] = f.read()
            else:
                log(f"  left out (not part of a moveset): {f.install_path}")
        elif len(parts) == 4 and lower[:2] == ["vfx", "chara"]:
            variant = variants.setdefault(parts[2].upper(), Variant(parts[2].upper()))
            variant.effects[parts[3]] = f.read()
            if lower[3] == parts[2].lower() + ".eepk":
                variant.eepk_name = parts[3]
        elif re.fullmatch(r"sound/se/battle/chara/car_btl_[a-z0-9]{3}_se\.(acb|awb)", key):
            code = parts[4][8:11].upper()
            variants.setdefault(code, Variant(code)).sound[os.path.splitext(key)[1]] = f.read()
        elif key in DROPPED_MERGES:
            log(f"  left out (new codes get their own entries): {f.install_path}")
        else:
            shared.append(f)
    for code in [c for c, v in variants.items() if not v.parts]:
        log(f"  left out (effects/sound without a moveset): {code}")
        del variants[code]
    return variants, shared, merges


# ---------------------------------------------------------------- output ---
def new_codes(name, count, used):
    """`count` free codes, starting at a spot picked from the style's name so
    re-running gives the same codes and different mods rarely meet."""
    total = len(BASE36) ** 2
    start = zlib.crc32(name.encode("utf-8")) % total
    codes = []
    for step in range(total):
        n = (start + step) % total
        code = CODE_PREFIX + BASE36[n // len(BASE36)] + BASE36[n % len(BASE36)]
        if code not in used:
            codes.append(code)
            used.add(code)
            if len(codes) == count:
                return codes
    sys.exit("No free style codes left")


def installed_style_codes(game, own_file):
    """(codes used by other installed styles, codes of this style's own
    earlier install) - code + movesets of each style XML. The own ones are
    in the game's CMS too, but free to reuse: a re-convert keeps its codes
    (and so the styles players equipped)."""
    folder = game.loose("battle_styles/styles")
    used, own = set(), set()
    if not os.path.isdir(folder):
        return used, own
    for name in os.listdir(folder):
        if not name.lower().endswith(".xml"):
            continue
        with open(os.path.join(folder, name), "rb") as f:
            codes = {code.decode("ascii", "replace").upper()
                     for code in re.findall(rb'(?:code|moveset)="([^"]{1,3})"', f.read())}
        (own if name.lower() == own_file.lower() else used).update(codes)
    return used, own


def cms_value(value, code):
    """A vanilla CMS path value as seen from chara/<new code>/ (the getter
    resolves "X" to chara/<entry code>/X, "../A/B" to chara/A/B)."""
    if not value or value.upper() == "NULL" or value.startswith("../"):
        return value
    return f"../{code}/{value}"


def safe_file_name(name):
    return re.sub(r'[<>:"/\\|?*\x00-\x1f]', "_", name).strip(" .") or "style"


def add_folder_entries(archive):
    """Add a "dir/" entry for every folder in the archive, as LB's own packer
    does. The installer only copies JUNGLE1/JUNGLE2 when the folder has its
    own entry (Install.cs JungleCheck -> ZipManager.Exists("JUNGLE1/") ->
    ZipArchive.GetEntry), so without them the style's files never install."""
    names = set(archive.namelist())
    folders = set()
    for name in names:
        parts = name.split("/")[:-1]
        for depth in range(1, len(parts) + 1):
            folders.add("/".join(parts[:depth]) + "/")
    for folder in sorted(folders - names):
        archive.mkdir(folder)


def read_cso_banks(data):
    """chara id -> SE bank name from system/chara_sound.cso ("#CSO", count at
    +8, table at +0xc, 32-byte entries: chara id +0, costume +4, SE name
    string offset +8 - acb_merge.cpp CharaSeBank). Costume 0's bank wins."""
    count, table = struct.unpack_from("<II", data, 8)
    banks = {}
    for i in range(count):
        chara_id, costume, name_offset = struct.unpack_from("<iII", data, table + i * 32)
        if name_offset and (chara_id not in banks or costume == 0):
            banks[chara_id] = data[name_offset:data.index(b"\0", name_offset)].decode("ascii", "replace")
    return banks


def game_se_bank(game, bank):
    """{".acb": bytes, ".awb": bytes} of the game's SE bank `bank` (awb only if it has one)."""
    found = {}
    for ext in (".acb", ".awb"):
        data = game.read(f"sound/SE/Battle/Chara/{bank}{ext}")
        if data:
            found[ext] = data
    return found if ".acb" in found else {}


# BCS layout (Xv2CoreLib BCS_File / prop_fit.cpp): part sets at header
# +12 (count, u16) / +24 (table of absolute offsets); a part set's 10 part
# offsets at its +24 (relative to the set; 0 = none); a part is 88 bytes,
# model number at +0 (i16), 4-byte chara code at +52, physics objects +74
# (count) / +76 (offset). Part files are "<code>_<model:000>_<part>".
BCS_PART_NAMES = ("Face_base", "Face_forehead", "Face_eye", "Face_nose", "Face_ear", "Hair", "Bust", "Pants",
                  "Rist", "Boots")


def emd_prop_bones(emd):
    """For each submesh of an EMD, the bone names its triangle lists use
    (EMD layout: prop_fit.cpp ForEachTriangle / tools\\battle_styles\\emdbones.py)."""
    model_count, model_table = struct.unpack_from("<H", emd, 18)[0], struct.unpack_from("<I", emd, 20)[0]
    for m in range(model_count):
        model = struct.unpack_from("<I", emd, model_table + 4 * m)[0]
        if not model:
            continue
        mesh_count, mesh_table = struct.unpack_from("<H", emd, model + 2)[0], struct.unpack_from("<I", emd, model + 4)[0]
        for k in range(mesh_count):
            mesh = model + struct.unpack_from("<I", emd, model + mesh_table + 4 * k)[0]
            sub_count, sub_table = struct.unpack_from("<H", emd, mesh + 54)[0], struct.unpack_from("<I", emd, mesh + 56)[0]
            for s in range(sub_count):
                sub = mesh + struct.unpack_from("<I", emd, mesh + sub_table + 4 * s)[0]
                list_count, list_table = struct.unpack_from("<H", emd, sub + 70)[0], struct.unpack_from("<I", emd, sub + 76)[0]
                bones = set()
                for l in range(list_count):
                    triangles = sub + struct.unpack_from("<I", emd, sub + list_table + 4 * l)[0]
                    _, name_count, _, names = struct.unpack_from("<IIII", emd, triangles)
                    for b in range(name_count):
                        at = triangles + struct.unpack_from("<I", emd, triangles + names + 4 * b)[0]
                        bones.add(emd[at:emd.index(b"\0", at)].decode("ascii", "replace"))
                yield bones


def holds_prop(emd):
    """An EMD Battle Styles can make a prop from (prop_fit.cpp): a submesh
    skinned to an a_x_* bone, or to nothing but a hand (Kaido's club,
    b_R_Hand only; the DLL decides by its length)."""
    try:
        return any(any(b.startswith("a_x_") for b in bones) or (bones and bones <= {"b_R_Hand", "b_L_Hand"})
                   for bones in emd_prop_bones(emd))
    except (struct.error, ValueError):
        return False


def x2m_prop_files(x2m, new_code):
    """The x2m character's model files a prop is cut from, under `new_code`:
    its BCS (codes renamed), its skeleton, and the first part set's parts
    whose EMD holds a prop. Only Battle Styles reads them - the game never
    spawns the style character. {} when the model has no prop."""
    own, model = x2m["code"], x2m["model"]
    bcs_path, esk_path = model.get(f"{own.lower()}.bcs"), model.get(f"{own.lower()}_000.esk")
    if not bcs_path or not esk_path:
        return {}
    bcs = open(bcs_path, "rb").read()
    try:
        set_count, set_table = struct.unpack_from("<H", bcs, 12)[0], struct.unpack_from("<I", bcs, 24)[0]
        first_set = struct.unpack_from("<I", bcs, set_table)[0] if set_count else 0
        part_table = first_set + struct.unpack_from("<I", bcs, first_set + 24)[0] if first_set else 0
        parts = [struct.unpack_from("<I", bcs, part_table + 4 * slot)[0] for slot in range(10)] if part_table else []
    except struct.error:
        return {}
    wanted = []
    for slot, part in enumerate(parts):
        if not part:
            continue
        number = struct.unpack_from("<h", bcs, first_set + part)[0]
        code = bcs[first_set + part + 52:first_set + part + 56].split(b"\0")[0].decode("ascii", "replace") or own
        stem = f"{code}_{number:03d}_{BCS_PART_NAMES[slot]}"
        emd = model.get(stem.lower() + ".emd")
        if number >= 0 and code.upper() == own.upper() and emd and holds_prop(open(emd, "rb").read()):
            wanted.append(stem)
    if not wanted:
        return {}
    # Same-length renames: "K7H\0" chara codes in the parts / physics objects,
    # "K7H_..." file names.
    renamed = bcs.replace(own.encode() + b"\0", new_code.encode() + b"\0").replace(
        own.encode() + b"_", new_code.encode() + b"_")
    out = {f"chara/{new_code}/{new_code}.bcs": renamed,
           f"chara/{new_code}/{new_code}_000.esk": open(esk_path, "rb").read()}
    for stem in wanted:
        for ext in (".emd", ".emm", ".emb", ".dyt.emb"):
            path = model.get(stem.lower() + ext)
            if path:
                out[f"chara/{new_code}/{new_code}{stem[len(own):]}{ext}"] = open(path, "rb").read()
    log(f"    prop model: {', '.join(wanted)} (as {new_code})")
    return out


def x2m_character(mod, files, cms, game):
    """Adds the x2m character's CMS entry to `cms` (paths to its own files,
    which only its moveset is taken from, become HUM's) and keeps from its
    JUNGLE only the character's effects; its SE bank is the one its CSO
    entry names, from JUNGLE or the game. Returns the files to classify."""
    x2m = mod.x2m
    own = x2m["code"]
    human = cms["HUM"]
    entry = dict(x2m["cms"])
    for field, _ in CMS_STRINGS:
        value = entry[field]
        if not value or value.upper() == "NULL":
            continue
        if not value.startswith("../") or value.upper().startswith(f"../{own}/"):
            entry[field] = cms_value(human[field], "HUM")  # the x2m's own model/face: not installed
    cms[own] = entry

    effects = f"vfx/chara/{own.lower()}/"
    bank_files = {f"sound/se/battle/chara/{x2m['se_bank'].lower()}{ext}": ext for ext in (".acb", ".awb")}
    kept, bank, dropped = [], {}, 0
    for f in files:
        key = f.install_path.lower()
        if f.origin != "x2m JUNGLE" or key.startswith(effects):
            kept.append(f)
        elif key in bank_files:
            bank[bank_files[key]] = f.read()
        else:
            dropped += 1
    if dropped:
        log(f"  {dropped} other x2m file(s) left out (skills, sounds, UI of the character)")
    if ".acb" not in bank and x2m["se_bank"]:
        bank = game_se_bank(game, x2m["se_bank"])
    x2m["sound"] = bank
    if x2m["se_bank"]:
        log(f"  sounds: {x2m['se_bank']}" + ("" if bank else " - not found, the race's sounds play"))
    return kept


def build(mod, out_root, game):
    info, files = mod.info, mod.files
    if not files:
        sys.exit("No moveset found: nothing in the mod is a moveset file "
                 "(<CODE>.ean, <CODE>_PLAYER.bac/.bcm/.bdm) - is it a costume or model mod?")

    name = mod.name
    log(f"Mod: {name} {info['version']} by {info['author'] or '?'}")

    cms_data = game.read("system/char_model_spec.cms")
    if not cms_data:
        sys.exit("The game's system/char_model_spec.cms was not found")
    cms = read_cms(cms_data)
    if mod.x2m:
        files = x2m_character(mod, files, cms, game)

    variants, shared, merges = classify(files)
    if not variants:
        sys.exit("No moveset found: the mod installs no chara/<CODE>/<CODE>.ean, _PLAYER.bac, .bcm or .bdm"
                 " - is it a costume or model mod?")

    if mod.x2m:
        variant = variants.get(mod.x2m["code"])
        if variant:
            variant.sound = variant.sound or mod.x2m["sound"]
            if not variant.eepk_name and mod.x2m["eepk"]:
                variant.eepk_fallback = mod.x2m["eepk"]  # e.g. chara/XBF/XBF.eepk: a roster character's
    # A roster character's moveset without its own SE bank keeps its sounds:
    # its CSO bank, copied under the new code (Battle Styles finds banks by
    # name). Races play their own sounds without one.
    cso_data = game.read("system/chara_sound.cso")
    cso = read_cso_banks(cso_data) if cso_data else {}
    for code, variant in variants.items():
        if code not in RACES and code in cms and ".acb" not in variant.sound and cms[code]["id"] in cso:
            variant.sound = game_se_bank(game, cso[cms[code]["id"]])
            if variant.sound:
                log(f"  {code} sounds: the game's {cso[cms[code]['id']]}")

    for code in [c for c in variants if c not in cms]:
        warn(f"{code} is not a character in the game's CMS - its moveset is left out")
        del variants[code]
    if not variants:
        sys.exit("No usable moveset left")

    # One new code per distinct variant, in race order then roster order.
    order = sorted(variants, key=lambda c: (RACES.index(c) if c in RACES else len(RACES), c))
    unique = {}
    for code in order:
        unique.setdefault(variants[code].signature(), []).append(code)
    style_file = safe_file_name(name) + ".xml"
    others, own_earlier = installed_style_codes(game, style_file)
    used = (set(cms) - own_earlier) | others
    codes = new_codes(name, len(unique), used)
    groups = []
    for new_code, members in zip(codes, unique.values()):
        for code in members:
            variants[code].code = new_code
        groups.append((new_code, [variants[c] for c in members]))

    jungle = {}           # data-relative path -> bytes (JUNGLE1: copied as is)
    cms_entries, ers_entries = [], []
    for index, (new_code, members) in enumerate(groups):
        variant = members[0]
        source = cms[variant.source_code]
        alias = f"BattleStyle_{new_code}"
        log(f"  {new_code} <- {', '.join(v.source_code for v in members)}"
            f" ({', '.join(sorted(variant.parts))}"
            f"{', effects' if variant.eepk_name else ''}{', sound' if '.acb' in variant.sound else ''})")

        # Moveset files, named after the new code.
        suffixes = {field: suffix for suffix, field in MOVESET_PARTS.items()}
        fields = {}
        for field, data in variant.parts.items():
            jungle[f"chara/{new_code}/{new_code}{suffixes[field].replace('_player', '_PLAYER')}"] = data
            fields[field] = new_code
        for field, _ in CMS_STRINGS:
            fields.setdefault(field, cms_value(source[field], variant.source_code))
        # an x2m character's held prop (Kaido's club): its model under the new
        # code, so Battle Styles can cut the prop from it
        if mod.x2m and variant.source_code == mod.x2m["code"]:
            prop_files = x2m_prop_files(mod.x2m, new_code)
            if prop_files:
                jungle.update(prop_files)
                fields["BCS"] = new_code

        # Effects: the mod's folder plus the race folder's assets it names but
        # didn't ship (the mod copied over the race's own folder).
        eepk_path = None
        if variant.eepk_name:
            vanilla = game.folder_files(f"vfx/chara/{variant.source_code}")
            shipped = {n.lower() for n in variant.effects}
            missing = []
            for file_name, data in variant.effects.items():
                target = f"{new_code}.eepk" if file_name == variant.eepk_name else file_name
                jungle[f"vfx/chara/{new_code}/{target}"] = data
            named = sorted({m.decode("ascii") for m in EFFECT_ASSET.findall(variant.effects[variant.eepk_name])})
            for asset in named:
                if asset.lower() in shipped:
                    continue
                if asset.lower() in vanilla:
                    jungle[f"vfx/chara/{new_code}/{asset}"] = game.read(vanilla[asset.lower()])
                    shipped.add(asset.lower())
                else:
                    missing.append(asset)
            # Particle (.emp) and trace (.etr) files live packed inside the
            # EEPK's "<name>.pbind.emb" / "<name>.tbind.emb" containers, so a
            # name found in one of those is not missing.
            packed = set()
            for container in (n for n in named if n.lower().endswith(("bind.emb",))):
                data = jungle.get(f"vfx/chara/{new_code}/{container}")
                if data:
                    packed |= {m.decode("ascii").lower() for m in EFFECT_ASSET.findall(data)}
            missing = [asset for asset in missing if asset.lower() not in packed]
            if missing:
                log(f"    {len(missing)} effect asset(s) the EEPK names are in neither the mod nor the game:"
                    f" {', '.join(missing)}")
            eepk_path = f"chara/{new_code}/{new_code}.eepk"
        elif variant.eepk_fallback and game.exists("vfx/" + variant.eepk_fallback):
            eepk_path = variant.eepk_fallback
        elif game.exists(f"vfx/chara/{variant.source_code}/{variant.source_code}.eepk"):
            eepk_path = f"chara/{variant.source_code}/{variant.source_code}.eepk"  # the race's own effects
        for ext, data in variant.sound.items():
            jungle[f"sound/SE/Battle/Chara/CAR_BTL_{new_code}_SE{ext}"] = data
        if ".acb" not in variant.sound:
            log(f"    no SE bank in the mod: {new_code} plays the race's sounds")

        id_binding = f"{{AutoCharaID=({CMS_ID_RANGE[0]};{CMS_ID_RANGE[1]}), SetAlias={alias}}}"
        lines = [f'  <Entry ID="{id_binding}" ShortName="{new_code}">']
        for field in ("I_08", "I_16", "LoadCamDist", "I_22", "I_24", "I_26", "I_28"):
            lines.append(f'    <{field} value="{source[field]}" />')
        for field in ("BCS", "EAN", "FCE_EAN", "FCE", "CAM_EAN", "BAC", "BCM", "BAI", "BDM"):
            lines.append(f"    <{field} value={quoteattr(fields[field])} />")
        lines.append("  </Entry>")
        cms_entries.append("\n".join(lines))
        if eepk_path:
            ers_entries.append(f'    <SubEntry ID="{{GetAlias={alias}}}" Name="">\n'
                               f'      <File string="{eepk_path}" />\n    </SubEntry>')

    # The style: one line per race the mod covers; a roster-only mod is "ANY".
    race_lines = [(v.source_code, v.code) for c, v in variants.items() if c in RACES]
    if not race_lines:
        race_lines = [("ANY", variants[order[0]].code)]
    race_lines.sort(key=lambda line: RACES.index(line[0]) if line[0] in RACES else -1)
    style_xml = (f'<?xml version="1.0" encoding="utf-8"?>\n'
                 f'<!-- Made by BattleStyles\\tools\\moveset_to_style.py from "{info["name"] or name}" -->\n'
                 f'<BattleStyle code="{codes[0]}" name={quoteattr(name)}>\n'
                 + "".join(f'  <Race race="{race}" moveset="{code}" />\n' for race, code in race_lines)
                 + "</BattleStyle>\n")
    jungle[f"battle_styles/styles/{style_file}"] = style_xml.encode("utf-8")

    for f in shared:
        jungle.setdefault(f.install_path, f.read())
        log(f"  kept as is: {f.install_path} ({f.origin})")
    for f in merges:
        log(f"  kept as is: {f.install_path} merge from {f.source_path} ({f.origin})")
        if b"{" in f.read():
            warn(f"{f.source_path} uses LB bindings - check it still installs without the original's other files")

    # ---- the installer
    lb_name = f"{name} (Battle Style)"  # the name LB tracks the install by: keep it stable
    exe = mod.exe or (BUNDLED_LB_EXE if os.path.isfile(BUNDLED_LB_EXE) else None)
    if out_root:
        out_dir = os.path.join(out_root, safe_file_name(f"Battle Style - {name}"))
        os.makedirs(out_dir, exist_ok=True)
        base = os.path.join(out_dir, safe_file_name(f"{lb_name} Installer"))
    else:  # next to the original: "<its exe name> (Battle Styles Compatible).exe"
        out_dir = mod.out_dir
        stem = os.path.splitext(os.path.basename(mod.exe))[0] if mod.exe else f"{name} Installer"
        base = os.path.join(out_dir, safe_file_name(stem + OUTPUT_TAG))
    file_xml = [
        '    <File>\n      <SourcePath value="battle_style/char_model_spec.cms.xml" />\n'
        '      <InstallPath value="system/char_model_spec.cms" />\n    </File>']
    if ers_entries:
        file_xml.append(
            '    <File>\n      <SourcePath value="battle_style/vfx_spec.ers.xml" />\n'
            '      <InstallPath value="vfx/vfx_spec.ers" />\n    </File>')
    for f in merges:
        type_attribute = "" if f.merge_type == "XML" and f.source_path.endswith(".xml") else f' Type="{f.merge_type}"'
        file_xml.append(f"    <File{type_attribute}>\n      <SourcePath value={quoteattr(f.source_path)} />\n"
                        f"      <InstallPath value={quoteattr(f.install_path)} />\n    </File>")
    ui = ET.tostring(info["ui"], encoding="unicode").strip() if info["ui"] is not None else ""
    message = (f"Adds {name} as a Battle Style (needs the Battle Styles mod). "
               f"Nothing in the game is replaced. Run this installer again to uninstall.")
    installer_xml = (
        '<?xml version="1.0" encoding="utf-8"?>\n<InstallerXml>\n'
        f'  <Name value={quoteattr(lb_name)} />\n  <Version value={quoteattr(info["version"])} />\n'
        f'  <Author value={quoteattr(info["author"] or "unknown")} />\n'
        + (f"  {ui}\n" if ui else "")
        + '  <InstallSteps>\n    <InstallStep>\n      <Type value="Message" />\n'
        f'      <StepName value="Battle Style" />\n      <Message value={quoteattr(message)} />\n'
        '    </InstallStep>\n  </InstallSteps>\n'
        '  <FilesToInstall>\n' + "\n".join(file_xml) + "\n  </FilesToInstall>\n</InstallerXml>\n")
    cms_xml = '<?xml version="1.0" encoding="utf-8"?>\n<CMS>\n' + "\n".join(cms_entries) + "\n</CMS>\n"
    ers_xml = ('<?xml version="1.0" encoding="utf-8"?>\n<ERS>\n  <Entry Type="2">\n'
               + "\n".join(ers_entries) + "\n  </Entry>\n</ERS>\n")

    with zipfile.ZipFile(base + ".installinfo", "w", zipfile.ZIP_DEFLATED) as out:
        out.writestr("InstallerXml.xml", installer_xml.encode("utf-8"))
        if info["archive"] is not None:  # UI art (BG.jpg ...): every root file but the xml
            for entry in info["archive"].namelist():
                if "/" not in entry and entry != "InstallerXml.xml":
                    out.writestr(entry, info["archive"].read(entry))
        out.writestr("data/battle_style/char_model_spec.cms.xml", cms_xml.encode("utf-8"))
        if ers_entries:
            out.writestr("data/battle_style/vfx_spec.ers.xml", ers_xml.encode("utf-8"))
        for f in merges:
            out.writestr("data/" + f.source_path, f.read())
        for path, data in sorted(jungle.items()):
            out.writestr("JUNGLE1/" + path, data)
        add_folder_entries(out)

    if exe:
        # A file copy, not read + write: on a StableBit DrivePool drive an exe
        # written that way crashed on start (.NET FileLoadException 0x80131621
        # loading it), while CopyFile copies of the same bytes ran.
        shutil.copy2(exe, base + ".exe")
    else:
        warn("no LB Mod Installer .exe (neither in the mod nor in tools\\lb_installer): "
             "copy one next to the .installinfo, "
             "named \"" + os.path.basename(base) + ".exe\"")
    log("")
    log(f"Style \"{name}\": " + ", ".join(f"{race}={code}" for race, code in race_lines))
    log(f"Written: {base}.exe")
    return base + ".exe" if exe else None


class Tee:
    """Writes to the console and a log file, so the output can be shared
    after the console window closes (Convert_Moveset (Drag installers here).bat)."""

    def __init__(self, console, log_file):
        self.console = console
        self.log_file = log_file

    def write(self, text):
        self.console.write(text)
        self.log_file.write(text)
        self.log_file.flush()

    def flush(self):
        self.console.flush()


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    log_path = os.path.join(HERE, "..", "Convert_Moveset.log")
    log_file = open(log_path, "w", encoding="utf-8")
    sys.stdout = Tee(sys.stdout, log_file)
    sys.stderr = Tee(sys.stderr, log_file)
    try:
        convert(sys.argv)
    except SystemExit as stop:
        if stop.code not in (None, 0):
            print(stop.code if isinstance(stop.code, str) else f"Stopped ({stop.code})", file=sys.stderr)
            print(f"\nLog: {os.path.normpath(log_path)}")
            raise SystemExit(1)
    except Exception:
        import traceback
        traceback.print_exc()
        print(f"\nConversion failed. Log: {os.path.normpath(log_path)}")
        raise SystemExit(1)


def convert(argv):
    run_installer = "--no-install" not in argv
    argv = [arg for arg in argv if arg != "--no-install"]
    target = os.path.abspath(argv[1])
    out_root = os.path.abspath(argv[2]) if len(argv) > 2 else None
    game_folder = find_game()
    if not game_folder or not os.path.isdir(os.path.join(game_folder, "cpk")):
        sys.exit("Game not found: set XV2_GAME to the DB Xenoverse 2 folder")
    scratch = tempfile.mkdtemp(prefix="moveset_to_style_")  # archives unpack here
    source = None
    try:
        source = read_mod(target, scratch)
        installer = build(source, out_root, Game(game_folder))
    finally:
        if source and source.info and source.info.get("archive") is not None:
            source.info["archive"].close()  # may be an .installinfo inside the scratch folder
        shutil.rmtree(scratch, ignore_errors=True)
    if installer and run_installer:
        log("Starting the installer...")
        # ShellExecute (shows the UAC prompt if the installer asks for admin),
        # started in its own folder: LB Mod Installer opens "<exe name>.installinfo"
        # relative to the working directory (MainWindow.LoadInstallInfoZip).
        # (chdir rather than startfile's cwd=, which needs Python 3.10+)
        os.chdir(os.path.dirname(installer))
        os.startfile(installer)


if __name__ == "__main__":
    main()
