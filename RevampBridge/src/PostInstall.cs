// Fixes applied after the Revamp installer has run, for game 1.26.01 or 1.27
// (GameBuild reads which from bin\DBXV2.exe; anything else is refused).
//
//  * XV2PATCHER\Epatches\RevampDynamicPartsets.xml / RevampExtra.xml are
//    replaced with that build's ports from epatches\<build>\ (port_epatches.py).
//  * data\rvp.cpk: Revamp's exe patch makes the game load it *instead of*
//    cpk\data2.cpk. Revamp built it from the 1.25.02 data2.cpk (online config,
//    system font) plus two files of its own. It is rebuilt from the current
//    data2.cpk plus those two files.
//  * data\ui\CharaImage\chara_image_TUnn.hci: the game reads the newest TU
//    table (TU22 in 1.26.01, TU23 in 1.27); Revamp only writes TU21. The newest
//    table is written as Revamp's TU21 plus the entries the updates added.
//  * Menu textures (data\ui\iggy, data\system\iggy): 1.26 and 1.27 renumbered the
//    textures of several menus; where Revamp ships only the textures or only
//    the menu, the other side is renumbered to match (see IggyTex).
//  * data\msg: entries the update added are appended and the lines' lost
//    parameter counts restored (see Msg).
//  * data\chara\CMN\*.lby.bac: lobby skill emotes Revamp's files lack are
//    copied from vanilla, and the flight speed set back to vanilla (see Bac).
//  * data\event\lobby\lobby_event_manage.tsd: the lobby events/triggers
//    Revamp patches by fixed 1.25.02 number are moved to their numbers in
//    the current build (see LobbyTsd).
//
// Uninstalling Revamp removes the Epatches, rvp.cpk and its texture files
// itself (they are in its install record); the bridge removes the files it
// created and puts back the lobby entries it moved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Text.RegularExpressions;

static class PostInstall
{
    const string Tracker = @"data\LB Mod Installer\revamp xenoverse 2 project_revamp team.xml";
    const string StateFile = @"data\XV2_RevampBridge.txt";

    // The LB installer rewrites its tracker xml on every install and
    // uninstall, so an unchanged stamp means the installer was closed
    // without doing either - the launcher then skips the fixes.
    public static string TrackerStamp(string game)
    {
        var t = new FileInfo(Path.Combine(game, Tracker));
        return t.Exists ? t.Length + "@" + t.LastWriteTimeUtc.Ticks : "missing";
    }

    public static bool RevampInstalled(string game)
    {
        string t = Path.Combine(game, Tracker);
        return File.Exists(t) && File.ReadAllText(t).Contains("<Mod ");
    }

    // The game build, from DBXV2.exe's PE header: {link time stamp, SizeOfImage}
    // of each supported build (read from the Steam exes). null = unsupported.
    static readonly Dictionary<string, uint[]> Builds = new Dictionary<string, uint[]>
    {
        { "1.26.01", new uint[] { 0x6A63732E, 0x1A1F000 } },
        { "1.27", new uint[] { 0x6AAA4CB1, 0x1A33000 } },
    };

    public static string GameBuild(string game)
    {
        var head = new byte[0x400];
        using (var f = File.OpenRead(Path.Combine(game, "bin", "DBXV2.exe"))) f.Read(head, 0, head.Length);
        int pe = BitConverter.ToInt32(head, 0x3C);
        if (pe < 0 || pe + 0x58 > head.Length) return null;
        uint stamp = BitConverter.ToUInt32(head, pe + 8);         // IMAGE_FILE_HEADER.TimeDateStamp
        uint image = BitConverter.ToUInt32(head, pe + 24 + 56);   // IMAGE_OPTIONAL_HEADER64.SizeOfImage
        return Builds.Where(b => b.Value[0] == stamp && b.Value[1] == image).Select(b => b.Key).FirstOrDefault();
    }

    // Returns a log; throws on failure.
    public static List<string> Run(string game, string bridgeDir)
    {
        var log = new List<string>();
        string build = GameBuild(game);
        if (build == null)
            throw new InvalidOperationException("This game version is not supported (the bridge knows " +
                string.Join(", ", Builds.Keys) + "). Nothing was changed.");
        log.Add("Game " + build);
        string state = Path.Combine(game, StateFile);
        var created = File.Exists(state) ? File.ReadAllLines(state).Where(l => l.Length > 0).ToList() : new List<string>();

        if (!RevampInstalled(game))
        {
            foreach (string rel in created)
            {
                string p = Path.Combine(game, rel);
                if (File.Exists(p)) { File.Delete(p); log.Add("Removed " + rel); }
            }
            // only after a bridge install (the state file exists), so another
            // mod's lobby edits are never touched
            if (File.Exists(state)) LobbyTsd.Restore(game, build, log);
            if (File.Exists(state)) File.Delete(state);
            if (log.Count == 1) log.Add("Revamp is not installed - nothing to do.");
            return log;
        }

        // 1. exe patches
        foreach (string src in Directory.GetFiles(Path.Combine(bridgeDir, "epatches", build), "*.xml"))
        {
            string dst = Path.Combine(game, "XV2PATCHER", "Epatches", Path.GetFileName(src));
            if (!File.Exists(dst)) continue;  // Revamp did not install this one
            if (File.ReadAllBytes(dst).SequenceEqual(File.ReadAllBytes(src))) { log.Add("Epatch up to date: " + Path.GetFileName(src)); continue; }
            File.Copy(src, dst, true);
            log.Add("Epatch ported to " + build + ": " + Path.GetFileName(src));
        }

        // 2. rvp.cpk
        string rvp = Path.Combine(game, "data", "rvp.cpk");
        if (File.Exists(rvp))
            log.Add(Cpk.RebuildRvp(rvp, Path.Combine(game, "cpk", "data2.cpk")));

        // 3. newest character image table
        string made = CharaImage.Update(game, log);
        if (made != null && !created.Contains(made)) created.Add(made);

        // 4. UI textures laid out for 1.25.02 menus
        IggyTex.Port(game, build, bridgeDir, created, log);

        // 5. text entries the update added
        Msg.AddMissing(game, log);

        // 6. lobby skill emotes Revamp's lobby actions lack
        Bac.AddMissingLobby(game, log);
        Bac.VanillaLobbyFlight(game, log);

        // 7. lobby events/triggers Revamp numbers as in 1.25.02
        LobbyTsd.Port(game, build, log);

        File.WriteAllLines(state, created);
        return log;
    }
}

// ------------------------------------------------------------------ MSG

// Revamp ships whole data\msg files built from 1.25.02 (plus its own
// entries), so names 1.26 added are missing: new characters show "Unknown
// Character" / "Missing MSG Entry", training settings lose their values. The
// game looks entries up by name (Revamp renumbers ids freely), so every
// vanilla entry a loose file lacks is appended; the mod's entries are kept.
//
// Revamp's installer also writes every line's parameter count as 0. The game
// then shows the line's "%ls" / "%d" as is ("You got TP Medals x %ls!",
// Mekuji's item request) - 1642 lines in 53 files, mostly lobby_text_*. The
// count cannot be derived from the text (vanilla has "Body Type %d" with 0:
// the game formats some lines itself), so vanilla's is copied by entry name
// and line where the loose line has the same format specifiers.
//
// File: "#MSG", u32 count at 8, name table at [0x0C] ({u32 offset, u32
// chars, u32 bytes, u32 0}), id table at [0x10] (u32), line table at [0x14]
// ({u32 lines, u32 offset of that many string records}), total lines at
// 0x18, string records at [0x1C] ({u32 offset, u32 chars, u32 bytes, u32
// parameter count}, vanilla 1.26: 0, 1 or 2); then names, then strings.
// Strings are kept as raw bytes.
static class Msg
{
    // Params: the record's 4th u32 (a line's parameter count; 0 for names).
    class Str { public byte[] Data; public uint Chars, Params; }
    class Entry { public Str Name; public uint Id; public List<Str> Lines = new List<Str>(); }

    static readonly Regex FormatSpecifier = new Regex(@"%[-+ 0#]*\d*(?:\.\d+)?(?:ls|lld|ld|s|d|u|x|f|c)");

    static string Specifiers(Str line)
    {
        return string.Join(" ", FormatSpecifier.Matches(Encoding.Unicode.GetString(line.Data)).Cast<Match>().Select(m => m.Value));
    }

    static Str ReadStr(byte[] b, int rec)
    {
        int off = BitConverter.ToInt32(b, rec), bytes = BitConverter.ToInt32(b, rec + 8);
        var s = new Str { Chars = BitConverter.ToUInt32(b, rec + 4), Params = BitConverter.ToUInt32(b, rec + 12), Data = new byte[bytes] };
        Buffer.BlockCopy(b, off, s.Data, 0, bytes);
        return s;
    }

    static List<Entry> Read(byte[] b, out byte[] head)
    {
        if (b.Length < 0x20 || Encoding.ASCII.GetString(b, 0, 4) != "#MSG") throw new InvalidDataException("not a .msg file");
        head = new byte[8]; Buffer.BlockCopy(b, 0, head, 0, 8);
        int n = BitConverter.ToInt32(b, 8), names = BitConverter.ToInt32(b, 0xC), ids = BitConverter.ToInt32(b, 0x10), lines = BitConverter.ToInt32(b, 0x14);
        var list = new List<Entry>(n);
        for (int i = 0; i < n; i++)
        {
            var e = new Entry { Name = ReadStr(b, names + 16 * i), Id = BitConverter.ToUInt32(b, ids + 4 * i) };
            int lc = BitConverter.ToInt32(b, lines + 8 * i), lo = BitConverter.ToInt32(b, lines + 8 * i + 4);
            for (int j = 0; j < lc; j++) e.Lines.Add(ReadStr(b, lo + 16 * j));
            list.Add(e);
        }
        return list;
    }

    static byte[] Write(byte[] head, List<Entry> list)
    {
        int n = list.Count, total = list.Sum(e => e.Lines.Count);
        int names = 0x20, ids = names + 16 * n, lines = ids + 4 * n, recs = lines + 8 * n, data = recs + 16 * total;
        int size = data + list.Sum(e => e.Name.Data.Length + e.Lines.Sum(l => l.Data.Length));
        var o = new byte[size];
        Buffer.BlockCopy(head, 0, o, 0, 8);
        Action<int, int> put = (pos, v) => BitConverter.GetBytes(v).CopyTo(o, pos);
        put(8, n); put(0xC, names); put(0x10, ids); put(0x14, lines); put(0x18, total); put(0x1C, recs);
        int pos2 = data;
        Action<int, Str> str = (rec, s) =>
        {
            put(rec, pos2); put(rec + 4, (int)s.Chars); put(rec + 8, s.Data.Length); put(rec + 12, (int)s.Params);
            Buffer.BlockCopy(s.Data, 0, o, pos2, s.Data.Length); pos2 += s.Data.Length;
        };
        for (int i = 0; i < n; i++) { str(names + 16 * i, list[i].Name); put(ids + 4 * i, (int)list[i].Id); }
        int r = recs;
        for (int i = 0; i < n; i++)
        {
            put(lines + 8 * i, list[i].Lines.Count); put(lines + 8 * i + 4, r);
            foreach (var l in list[i].Lines) { str(r, l); r += 16; }
        }
        return o;
    }

    static string Key(Entry e) { return Convert.ToBase64String(e.Name.Data); }

    static string Dump(List<Entry> list)
    {
        var sb = new StringBuilder();
        foreach (var e in list)
        {
            sb.Append(Key(e)).Append(':').Append(e.Name.Chars).Append(':').Append(e.Id);
            foreach (var l in e.Lines) sb.Append('|').Append(l.Chars).Append(':').Append(l.Params).Append(':').Append(Convert.ToBase64String(l.Data));
            sb.Append('\n');
        }
        return sb.ToString();
    }

    // Self-check: what the writer writes reads back as the same entries
    // (vanilla files are reproduced byte for byte, except that some order
    // their string record table differently).
    public static int RoundTrip(CpkSet cpk, IEnumerable<string> names)
    {
        int bad = 0;
        foreach (string k in names)
        {
            byte[] h, h2, d = cpk.Read(k);
            var list = Read(d, out h);
            if (Dump(Read(Write(h, list), out h2)) != Dump(list) || !h2.SequenceEqual(h)) bad++;
        }
        return bad;
    }

    public static void AddMissing(string game, List<string> log)
    {
        string dir = Path.Combine(game, "data", "msg");
        if (!Directory.Exists(dir)) return;
        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        int files = 0, added = 0, paramFiles = 0, paramLines = 0;
        foreach (string path in Directory.GetFiles(dir, "*.msg"))
        {
            string k = "data/msg/" + Path.GetFileName(path);
            if (!cpk.Exists(k)) continue;
            byte[] vanBytes = cpk.Read(k), modBytes = File.ReadAllBytes(path);
            if (modBytes.SequenceEqual(vanBytes)) continue;
            byte[] head, vh;
            var mod = Read(modBytes, out head);
            var van = Read(vanBytes, out vh);
            int restored = RestoreParams(mod, van);
            var have = new HashSet<string>(mod.Select(Key));
            var missing = van.Where(e => !have.Contains(Key(e))).ToList();
            if (missing.Count == 0 && restored == 0) continue;
            var used = new HashSet<uint>(mod.Select(e => e.Id));
            uint next = mod.Count == 0 ? 0 : mod.Max(e => e.Id) + 1;
            foreach (var e in missing)
            {
                if (used.Contains(e.Id)) e.Id = next++;
                used.Add(e.Id);
                mod.Add(e);
            }
            byte[] outBytes = Write(head, mod), h2;
            if (Dump(Read(outBytes, out h2)) != Dump(mod)) throw new InvalidDataException("Text writer check failed for " + k);
            File.WriteAllBytes(path, outBytes);
            if (missing.Count > 0) { files++; added += missing.Count; }
            if (restored > 0) { paramFiles++; paramLines += restored; }
        }
        log.Add(files > 0 ? "Text: " + added + " entries the update added, put into " + files + " of Revamp's .msg files."
                          : "Text: Revamp's .msg files have every entry the game's have.");
        if (paramFiles > 0) log.Add("Text: parameter counts restored on " + paramLines + " lines in " + paramFiles + " .msg files (\"%ls\" shown as is).");
    }

    // Copies vanilla's parameter count onto the mod's lines that lost it, by
    // entry name and line index, where both lines have the same specifiers.
    // Returns the number of lines changed.
    static int RestoreParams(List<Entry> mod, List<Entry> van)
    {
        var vanByName = new Dictionary<string, Entry>();
        foreach (var e in van) vanByName[Key(e)] = e;
        int changed = 0;
        foreach (var e in mod)
        {
            Entry vanEntry;
            if (!vanByName.TryGetValue(Key(e), out vanEntry)) continue;
            for (int j = 0; j < e.Lines.Count && j < vanEntry.Lines.Count; j++)
            {
                Str line = e.Lines[j], vanLine = vanEntry.Lines[j];
                if (line.Params == vanLine.Params || Specifiers(line) != Specifiers(vanLine)) continue;
                line.Params = vanLine.Params;
                changed++;
            }
        }
        return changed;
    }
}

// ------------------------------------------------------------------ BAC

// Revamp's lobby action files (CMN.HUM/HUF/MAM/NMC/SYF.lby.bac) were built
// from an older game: entries 416-419, the lobby skill emotes (Kamehameha,
// Special Beam Cannon, ...), are empty, and using one in an online lobby
// freezes the player forever. Entries that are empty in the loose file but
// used in vanilla are copied from vanilla; the mod's entries are kept.
//
// Revamp's lobby files (HUM HUF MAF MAM NMC SYF SYM) also make flight x1.4
// faster: entries 10/11/12 (take-off, boost loop, stop), Movement (type 2)
// Direction Z / Drag Z. Boost flying (L2) in Conton City then stops the
// player's movement every ~5 s (seen in game; gone with vanilla speed), so
// those Movement items are set back to vanilla's.
//
// File: "#BAC", u32 entry count at 8, entry table at [0x10]. Entry (16
// bytes): u32 flags (0x80000000 = empty), u16 type count, u16 0, u64 offset
// of its type table. Type header (16 bytes): u16 type, u16 item count, u32 0,
// u64 offset of the items. A copied entry's type table and item blocks are
// appended; a block's size is the distance to the next offset in vanilla.
static class Bac
{
    static void Check(byte[] b)
    {
        if (b.Length < 0x18 || Encoding.ASCII.GetString(b, 0, 4) != "#BAC") throw new InvalidDataException("not a .bac file");
    }

    static int Count(byte[] b) { return BitConverter.ToInt32(b, 8); }
    static int Entry(byte[] b, int i) { return BitConverter.ToInt32(b, 0x10) + 16 * i; }
    static int Types(byte[] b, int i) { return BitConverter.ToUInt16(b, Entry(b, i) + 4); }
    static int TypeTable(byte[] b, int i) { return (int)BitConverter.ToInt64(b, Entry(b, i) + 8); }

    // Every offset the file points at, sorted, to size the blocks.
    static List<int> Offsets(byte[] b)
    {
        var s = new SortedSet<int> { b.Length };
        for (int i = 0; i < Count(b); i++)
        {
            int n = Types(b, i);
            if (n == 0) continue;
            int t = TypeTable(b, i);
            s.Add(t);
            for (int j = 0; j < n; j++) s.Add((int)BitConverter.ToInt64(b, t + 16 * j + 8));
        }
        return s.ToList();
    }

    static int BlockSize(List<int> offsets, int at) { return offsets[offsets.BinarySearch(at) + 1] - at; }

    public static void AddMissingLobby(string game, List<string> log)
    {
        string dir = Path.Combine(game, "data", "chara", "CMN");
        if (!Directory.Exists(dir)) return;
        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        int files = 0, added = 0;
        foreach (string path in Directory.GetFiles(dir, "*.lby.bac"))
        {
            string k = "data/chara/CMN/" + Path.GetFileName(path);
            if (!cpk.Exists(k)) continue;
            byte[] van = cpk.Read(k), mod = File.ReadAllBytes(path);
            Check(van); Check(mod);
            if (Count(mod) != Count(van)) continue;  // not laid out like vanilla - leave it
            var missing = Enumerable.Range(0, Count(van)).Where(i => Types(van, i) > 0 && Types(mod, i) == 0).ToList();
            if (missing.Count == 0) continue;

            var offsets = Offsets(van);
            var o = new MemoryStream();
            o.Write(mod, 0, mod.Length);
            Action align = () => { while (o.Length % 16 != 0) o.WriteByte(0); };
            foreach (int i in missing)
            {
                int n = Types(van, i), vt = TypeTable(van, i);
                align();
                long table = o.Length;
                var tableBytes = new byte[16 * n];
                Buffer.BlockCopy(van, vt, tableBytes, 0, tableBytes.Length);
                o.Write(tableBytes, 0, tableBytes.Length);
                for (int j = 0; j < n; j++)
                {
                    int at = (int)BitConverter.ToInt64(van, vt + 16 * j + 8);
                    align();
                    long pos = o.Length;
                    o.Write(van, at, BlockSize(offsets, at));
                    BitConverter.GetBytes(pos).CopyTo(tableBytes, 16 * j + 8);
                }
                long end = o.Length;
                o.Position = table; o.Write(tableBytes, 0, tableBytes.Length); o.Position = end;
                var entry = new byte[16];
                Buffer.BlockCopy(van, Entry(van, i), entry, 0, 16);
                BitConverter.GetBytes(table).CopyTo(entry, 8);
                o.Position = Entry(mod, i); o.Write(entry, 0, 16); o.Position = end;
            }
            byte[] outBytes = o.ToArray();

            // Self-check: the copied entries read back byte for byte as vanilla's.
            var outOffsets = Offsets(outBytes);
            foreach (int i in missing)
            {
                int n = Types(outBytes, i), t = TypeTable(outBytes, i), vt = TypeTable(van, i);
                bool same = n == Types(van, i) && BitConverter.ToUInt32(outBytes, Entry(outBytes, i)) == BitConverter.ToUInt32(van, Entry(van, i));
                for (int j = 0; same && j < n; j++)
                {
                    int a = (int)BitConverter.ToInt64(outBytes, t + 16 * j + 8), va = (int)BitConverter.ToInt64(van, vt + 16 * j + 8);
                    same = outBytes.Skip(t + 16 * j).Take(8).SequenceEqual(van.Skip(vt + 16 * j).Take(8))
                        && outBytes.Skip(a).Take(BlockSize(offsets, va)).SequenceEqual(van.Skip(va).Take(BlockSize(offsets, va)));
                }
                if (!same) throw new InvalidDataException("Lobby action writer check failed for " + k);
            }
            File.WriteAllBytes(path, outBytes);
            files++; added += missing.Count;
        }
        log.Add(files > 0 ? "Lobby skill emotes: " + added + " entries copied from the game into " + files + " of Revamp's lobby action files."
                          : "Lobby skill emotes: Revamp's lobby action files have every entry the game's have.");
    }

    static readonly int[] FlightEntries = { 10, 11, 12 };  // take-off, boost loop, stop
    const int MovementType = 2;
    const int MovementItemSize = 36;  // 8-byte item header + Xv2CoreLib BAC_Type2 fields

    // (offset, size) of an entry's Movement items, or null.
    static int[] MovementBlock(byte[] b, int i)
    {
        int table = TypeTable(b, i);
        for (int j = 0; j < Types(b, i); j++)
            if (BitConverter.ToUInt16(b, table + 16 * j) == MovementType)
                return new[] { (int)BitConverter.ToInt64(b, table + 16 * j + 8), BitConverter.ToUInt16(b, table + 16 * j + 2) * MovementItemSize };
        return null;
    }

    // Overwrites the flight entries' Movement items with vanilla's where both
    // have the same item count (only the floats differ); nothing else moves.
    public static void VanillaLobbyFlight(string game, List<string> log)
    {
        string dir = Path.Combine(game, "data", "chara", "CMN");
        if (!Directory.Exists(dir)) return;
        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        var fixedFiles = new List<string>();
        foreach (string path in Directory.GetFiles(dir, "*.lby.bac"))
        {
            string k = "data/chara/CMN/" + Path.GetFileName(path);
            if (!cpk.Exists(k)) continue;
            byte[] van = cpk.Read(k), mod = File.ReadAllBytes(path);
            Check(van); Check(mod);
            bool changed = false;
            foreach (int i in FlightEntries)
            {
                if (i >= Count(van) || i >= Count(mod)) continue;
                int[] mine = MovementBlock(mod, i), theirs = MovementBlock(van, i);
                if (mine == null || theirs == null || mine[1] != theirs[1]) continue;
                if (mod.Skip(mine[0]).Take(mine[1]).SequenceEqual(van.Skip(theirs[0]).Take(theirs[1]))) continue;
                Buffer.BlockCopy(van, theirs[0], mod, mine[0], mine[1]);
                changed = true;
            }
            if (!changed) continue;
            File.WriteAllBytes(path, mod);
            fixedFiles.Add(Path.GetFileName(path));
        }
        log.Add(fixedFiles.Count > 0 ? "Lobby flight speed set back to vanilla (Revamp's x1.4 froze boost flight): " + string.Join(", ", fixedFiles)
                                     : "Lobby flight speed: already vanilla.");
    }
}

// ------------------------------------------------------------------ TSD

// Revamp's lobby XMLs (data/main/event/lobby_event_{events,triggers}.tsd.xml
// in the .installinfo) replace 26 events and 76 triggers of
// lobby_event_manage.tsd by fixed number, and those triggers name fixed event
// and lobby NPC numbers - all as numbered in 1.25.02. 1.26.01 inserted
// entries before them (events +2..+23, triggers +1..+73, NPCs 1271-1290 +18),
// so on 1.26 Revamp overwrites unrelated entries while the ones it meant to
// change keep pointing at shifted events. Seen in game: Future Gohan's "I want
// to train with you" opens the lesson menu with no lesson and the game
// crashes (DBXV2.exe+0x59dbcf reads an empty quest list). Also affected:
// Android 16's last lesson result and gift, the XV1 quest robot, the DLC story
// receptions, the Budokai announcers, Revamp's Advancement Test 5 robot.
//
// Each of Revamp's entries is moved to its 1.26 number (event and NPC
// references translated) and vanilla's entry goes back where Revamp wrote it.
// The tables come from tools\tsdmap.py, which matches every
// entry by content between the vanilla 1.25.02 and 1.26.01 files.
//
// File: byte 1, triggers, 8-byte end marker, byte 2, events, end marker,
// sections 3-5 (kept as they are). Trigger: i32 ID, DLC flag, icon, type,
// TNL_ID (+16), TNL sub id, Event_ID (+24), i32, i32 condition length (+32),
// condition. Event: i32 ID, i32, five strings (name, path, script, function,
// args) as i32 length + bytes, i32 count + that many i32 TNL ids.
static class LobbyTsd
{
    const string Name = "data/event/lobby/lobby_event_manage.tsd";
    const long EndMarker = -8526495041129795056;   // ends sections 1-4 (XV2-Tools TSD parser)
    const int FirstNewTriggerId = 10000;            // Revamp's new triggers: ID="{AutoID=(10000; 20000)}"

    // {1.25.02 number Revamp writes, the same vanilla entry's number in that
    // game build}, per build (tools\tsdmap.py prints them):
    //   Event: Future Gohan's mentor events, the XV1 quest robot's receptions
    //   EventRef: event numbers Revamp's moved triggers point at (vanilla events it does not write)
    //   Npc: lobby NPC numbers in Revamp's triggers (the DLC 13-17 story receptions)
    //   NewTriggerEvent: fixed event numbers in Revamp's new triggers (604: its Advancement Test 5 robot)
    class Tables { public int[,] Event, Trigger, EventRef, Npc, NewTriggerEvent; }

    // same in 1.26.01 and 1.27
    static readonly int[,] EventMoves =
    {
            { 553, 555 }, { 554, 556 }, { 555, 557 }, { 556, 558 }, { 557, 559 }, { 558, 560 }, { 559, 561 }, { 561, 563 },
            { 1973, 1975 }, { 1974, 1976 }, { 1975, 1977 }, { 1976, 1978 }, { 1977, 1979 }, { 1978, 1980 }, { 1979, 1981 }, { 1980, 1982 },
            { 1981, 1983 }, { 1982, 1984 }, { 1983, 1985 }, { 1984, 1986 }, { 1985, 1987 }, { 1986, 1988 }, { 1987, 1989 }, { 1988, 1990 },
            { 1989, 1991 }, { 1990, 1992 }
        };
    static readonly int[,] NewTriggerEventMoves = { { 604, 606 } };

    static readonly Dictionary<string, Tables> ByBuild = new Dictionary<string, Tables>
    {
        { "1.26.01", new Tables {
            Event = EventMoves, NewTriggerEvent = NewTriggerEventMoves,
            Trigger = new int[,]
            {
                { 2806, 2807 }, { 2807, 2808 }, { 2808, 2809 }, { 2809, 2810 }, { 2810, 2811 }, { 2813, 2814 }, { 2814, 2815 }, { 2815, 2816 },
                { 2816, 2817 }, { 2817, 2818 }, { 2818, 2819 }, { 2919, 2920 }, { 2920, 2921 }, { 2921, 2922 }, { 2922, 2923 }, { 2923, 2924 },
                { 2924, 2925 }, { 2925, 2926 }, { 2926, 2927 }, { 2927, 2928 }, { 2928, 2929 }, { 2929, 2930 }, { 2930, 2931 }, { 2931, 2932 },
                { 2932, 2933 }, { 2996, 2997 }, { 2997, 2998 }, { 2998, 2999 }, { 2999, 3000 }, { 3000, 3001 }, { 3001, 3002 }, { 3002, 3003 },
                { 3003, 3004 }, { 3004, 3005 }, { 3005, 3006 }, { 3006, 3007 }, { 3007, 3008 }, { 3008, 3009 }, { 3009, 3010 }, { 3943, 4016 },
                { 3944, 4017 }, { 3945, 4018 }, { 3946, 4019 }, { 3947, 4020 }, { 3948, 4021 }, { 3949, 4022 }, { 3950, 4023 }, { 3951, 4024 },
                { 3952, 4025 }, { 3953, 4026 }, { 3954, 4027 }, { 3955, 4028 }, { 3956, 4029 }, { 3957, 4030 }, { 3958, 4031 }, { 3959, 4032 },
                { 3960, 4033 }, { 3961, 4034 }, { 3962, 4035 }, { 3963, 4036 }, { 3964, 4037 }, { 3965, 4038 }, { 3966, 4039 }, { 3967, 4040 },
                { 3968, 4041 }, { 3970, 4043 }, { 3971, 4044 }, { 3972, 4045 }, { 3973, 4046 }, { 3974, 4047 }, { 3975, 4048 }, { 3976, 4049 },
                { 3977, 4050 }, { 3978, 4051 }, { 3979, 4052 }, { 3980, 4053 }
            },
            EventRef = new int[,]
            {
                { 1959, 1961 }, { 1960, 1962 }, { 1961, 1963 }, { 1962, 1964 }, { 1963, 1965 }, { 1966, 1968 }, { 1967, 1969 }, { 1968, 1970 },
                { 1969, 1971 }, { 1970, 1972 }, { 1971, 1973 }, { 2071, 2073 }, { 2072, 2074 }, { 2099, 2101 }, { 2100, 2102 }, { 2101, 2103 },
                { 2102, 2104 }, { 2114, 2116 }, { 2115, 2117 }, { 2116, 2118 }, { 2118, 2120 }, { 2119, 2121 }, { 2120, 2122 }, { 2121, 2123 },
                { 2122, 2124 }, { 2123, 2125 }, { 2435, 2458 }, { 2436, 2459 }, { 2437, 2460 }, { 2438, 2461 }, { 2439, 2462 }, { 2440, 2463 },
                { 2441, 2464 }, { 2442, 2465 }, { 2443, 2466 }, { 2444, 2467 }, { 2445, 2468 }, { 2446, 2469 }, { 2447, 2470 }, { 2465, 2488 },
                { 2466, 2489 }, { 2467, 2490 }, { 2468, 2491 }, { 2470, 2493 }, { 2471, 2494 }, { 2472, 2495 }, { 2473, 2496 }, { 2474, 2497 },
                { 2475, 2498 }, { 2476, 2499 }, { 2479, 2502 }, { 2481, 2504 }, { 2482, 2505 }, { 2483, 2506 }, { 2484, 2507 }, { 2485, 2508 },
                { 2486, 2509 }
            },
            Npc = new int[,]
            {
                { 1271, 1289 }, { 1272, 1290 }, { 1273, 1291 }, { 1274, 1292 }, { 1275, 1293 }, { 1276, 1294 }, { 1277, 1295 }, { 1287, 1305 },
                { 1288, 1306 }, { 1289, 1307 }, { 1290, 1308 }
            } } },
        { "1.27", new Tables {
            Event = EventMoves, NewTriggerEvent = NewTriggerEventMoves,
            Trigger = new int[,]
            {
                { 2806, 2807 }, { 2807, 2808 }, { 2808, 2809 }, { 2809, 2810 }, { 2810, 2811 }, { 2813, 2814 }, { 2814, 2815 }, { 2815, 2816 },
                { 2816, 2817 }, { 2817, 2818 }, { 2818, 2819 }, { 2919, 2920 }, { 2920, 2921 }, { 2921, 2922 }, { 2922, 2923 }, { 2923, 2924 },
                { 2924, 2925 }, { 2925, 2926 }, { 2926, 2927 }, { 2927, 2928 }, { 2928, 2929 }, { 2929, 2930 }, { 2930, 2931 }, { 2931, 2932 },
                { 2932, 2933 }, { 2996, 2997 }, { 2997, 2998 }, { 2998, 2999 }, { 2999, 3000 }, { 3000, 3001 }, { 3001, 3002 }, { 3002, 3003 },
                { 3003, 3004 }, { 3004, 3005 }, { 3005, 3006 }, { 3006, 3007 }, { 3007, 3008 }, { 3008, 3009 }, { 3009, 3010 }, { 3943, 4091 },
                { 3944, 4092 }, { 3945, 4093 }, { 3946, 4094 }, { 3947, 4095 }, { 3948, 4096 }, { 3949, 4097 }, { 3950, 4098 }, { 3951, 4099 },
                { 3952, 4100 }, { 3953, 4101 }, { 3954, 4102 }, { 3955, 4103 }, { 3956, 4104 }, { 3957, 4105 }, { 3958, 4106 }, { 3959, 4107 },
                { 3960, 4108 }, { 3961, 4109 }, { 3962, 4110 }, { 3963, 4111 }, { 3964, 4112 }, { 3965, 4113 }, { 3966, 4114 }, { 3967, 4115 },
                { 3968, 4116 }, { 3970, 4118 }, { 3971, 4119 }, { 3972, 4120 }, { 3973, 4121 }, { 3974, 4122 }, { 3975, 4123 }, { 3976, 4124 },
                { 3977, 4125 }, { 3978, 4126 }, { 3979, 4127 }, { 3980, 4128 }
            },
            EventRef = new int[,]
            {
                { 1959, 1961 }, { 1960, 1962 }, { 1961, 1963 }, { 1962, 1964 }, { 1963, 1965 }, { 1966, 1968 }, { 1967, 1969 }, { 1968, 1970 },
                { 1969, 1971 }, { 1970, 1972 }, { 1971, 1973 }, { 2071, 2073 }, { 2072, 2074 }, { 2099, 2101 }, { 2100, 2102 }, { 2101, 2103 },
                { 2102, 2104 }, { 2114, 2116 }, { 2115, 2117 }, { 2116, 2118 }, { 2118, 2120 }, { 2119, 2121 }, { 2120, 2122 }, { 2121, 2123 },
                { 2122, 2124 }, { 2123, 2125 }, { 2435, 2479 }, { 2436, 2480 }, { 2437, 2481 }, { 2438, 2482 }, { 2439, 2483 }, { 2440, 2484 },
                { 2441, 2485 }, { 2442, 2486 }, { 2443, 2487 }, { 2444, 2488 }, { 2445, 2489 }, { 2446, 2490 }, { 2447, 2491 }, { 2465, 2509 },
                { 2466, 2510 }, { 2467, 2511 }, { 2468, 2512 }, { 2470, 2514 }, { 2471, 2515 }, { 2472, 2516 }, { 2473, 2517 }, { 2474, 2518 },
                { 2475, 2519 }, { 2476, 2520 }, { 2479, 2523 }, { 2481, 2525 }, { 2482, 2526 }, { 2483, 2527 }, { 2484, 2528 }, { 2485, 2529 },
                { 2486, 2530 }
            },
            Npc = new int[,]
            {
                { 1271, 1310 }, { 1272, 1311 }, { 1273, 1312 }, { 1274, 1313 }, { 1275, 1314 }, { 1276, 1315 }, { 1277, 1316 }, { 1287, 1326 },
                { 1288, 1327 }, { 1289, 1328 }, { 1290, 1329 }
            } } },
    };

    class Tsd
    {
        public List<byte[]> Triggers = new List<byte[]>(), Events = new List<byte[]>();
        public byte[] Rest;   // from the end marker after the events to the end of the file

        public int TriggerIndex(int id) { return Triggers.FindIndex(r => Id(r) == id); }
        public int EventIndex(int id) { return Events.FindIndex(r => Id(r) == id); }

        public byte[] ToBytes()
        {
            var o = new MemoryStream();
            o.WriteByte(1);
            foreach (byte[] r in Triggers) o.Write(r, 0, r.Length);
            o.Write(BitConverter.GetBytes(EndMarker), 0, 8);
            o.WriteByte(2);
            foreach (byte[] r in Events) o.Write(r, 0, r.Length);
            o.Write(Rest, 0, Rest.Length);
            return o.ToArray();
        }
    }

    static int I32(byte[] b, int at) { return BitConverter.ToInt32(b, at); }
    static void SetI32(byte[] b, int at, int v) { BitConverter.GetBytes(v).CopyTo(b, at); }
    static int Id(byte[] record) { return I32(record, 0); }
    static bool AtEnd(byte[] b, int at) { return at + 8 > b.Length || BitConverter.ToInt64(b, at) == EndMarker; }
    static byte[] Slice(byte[] b, int at, int size) { var r = new byte[size]; Buffer.BlockCopy(b, at, r, 0, size); return r; }

    static Tsd Parse(byte[] b)
    {
        var t = new Tsd();
        int at = 0;
        if (b.Length < 1 || b[at++] != 1) throw new InvalidDataException("lobby_event_manage.tsd: triggers not first");
        while (!AtEnd(b, at))
        {
            int size = 36 + I32(b, at + 32);
            t.Triggers.Add(Slice(b, at, size));
            at += size;
        }
        at += 8;
        if (at >= b.Length || b[at++] != 2) throw new InvalidDataException("lobby_event_manage.tsd: events not second");
        while (!AtEnd(b, at))
        {
            int end = at + 8;
            for (int s = 0; s < 5; s++) end += 4 + I32(b, end);
            end += 4 + 4 * I32(b, end);
            t.Events.Add(Slice(b, at, end - at));
            at = end;
        }
        t.Rest = Slice(b, at, b.Length - at);
        return t;
    }

    static int Lookup(int[,] table, int key)
    {
        for (int i = 0; i < table.GetLength(0); i++) if (table[i, 0] == key) return table[i, 1];
        return key;
    }

    static IEnumerable<int> Column(int[,] table, int column)
    {
        for (int i = 0; i < table.GetLength(0); i++) yield return table[i, column];
    }

    static string LoosePath(string game) { return Path.Combine(game, "data", "event", "lobby", "lobby_event_manage.tsd"); }

    public static void Port(string game, string build, List<string> log)
    {
        string path = LoosePath(game);
        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        if (!File.Exists(path) || !cpk.Exists(Name)) return;
        Tables m = ByBuild[build];
        Tsd van = Parse(cpk.Read(Name)), mod = Parse(File.ReadAllBytes(path));
        Func<List<byte[]>, int, byte[]> vanilla = (list, id) =>
        {
            byte[] r = list.Find(x => Id(x) == id);
            if (r == null) throw new InvalidDataException("lobby_event_manage.tsd: the game's file has no entry " + id);
            return r;
        };

        // Revamp writes every 1.25 number; those nothing is moved onto hold
        // vanilla's entry again once ported - all of them, or none.
        var eventOnlySources = Column(EventMoves, 0).Except(Column(EventMoves, 1)).ToList();
        var triggerOnlySources = Column(m.Trigger, 0).Except(Column(m.Trigger, 1)).ToList();
        int stillRevamp = eventOnlySources.Count(id => mod.EventIndex(id) < 0 || !mod.Events[mod.EventIndex(id)].SequenceEqual(vanilla(van.Events, id)))
                        + triggerOnlySources.Count(id => mod.TriggerIndex(id) < 0 || !mod.Triggers[mod.TriggerIndex(id)].SequenceEqual(vanilla(van.Triggers, id)));
        if (stillRevamp == 0)
        {
            log.Add("Lobby events: Revamp's entries already have their " + build + " numbers.");
            return;
        }
        if (stillRevamp != eventOnlySources.Count + triggerOnlySources.Count)
            throw new InvalidDataException("lobby_event_manage.tsd: only " + stillRevamp + " of Revamp's " +
                (eventOnlySources.Count + triggerOnlySources.Count) + " fixed-number entries found - not laid out as Revamp 5.1.1 installs it; left unchanged");

        // Take Revamp's entries out at their 1.25 numbers, renumbered.
        var movedEvents = new Dictionary<int, byte[]>();
        for (int i = 0; i < EventMoves.GetLength(0); i++)
        {
            byte[] r = (byte[])mod.Events[mod.EventIndex(EventMoves[i, 0])].Clone();
            SetI32(r, 0, EventMoves[i, 1]);
            movedEvents[EventMoves[i, 1]] = r;
        }
        var movedTriggers = new Dictionary<int, byte[]>();
        for (int i = 0; i < m.Trigger.GetLength(0); i++)
        {
            byte[] r = (byte[])mod.Triggers[mod.TriggerIndex(m.Trigger[i, 0])].Clone();
            SetI32(r, 0, m.Trigger[i, 1]);
            SetI32(r, 16, Lookup(m.Npc, I32(r, 16)));
            SetI32(r, 24, Lookup(m.EventRef, I32(r, 24)));
            movedTriggers[m.Trigger[i, 1]] = r;
        }

        // Vanilla back at the 1.25 numbers, then Revamp's at the new ones.
        foreach (int id in Column(EventMoves, 0)) mod.Events[mod.EventIndex(id)] = vanilla(van.Events, id);
        foreach (int id in Column(m.Trigger, 0)) mod.Triggers[mod.TriggerIndex(id)] = vanilla(van.Triggers, id);
        foreach (var e in movedEvents)
        {
            int at = mod.EventIndex(e.Key);
            if (at < 0) throw new InvalidDataException("lobby_event_manage.tsd: no event " + e.Key);
            mod.Events[at] = e.Value;
        }
        foreach (var t in movedTriggers)
        {
            int at = mod.TriggerIndex(t.Key);
            if (at < 0) throw new InvalidDataException("lobby_event_manage.tsd: no trigger " + t.Key);
            mod.Triggers[at] = t.Value;
        }
        int newTriggersFixed = 0;
        foreach (byte[] r in mod.Triggers.Where(r => Id(r) >= FirstNewTriggerId))
        {
            int to = Lookup(NewTriggerEventMoves, I32(r, 24));
            if (to != I32(r, 24)) { SetI32(r, 24, to); newTriggersFixed++; }
        }

        // Self-check: the written file reads back with every entry in place.
        byte[] outBytes = mod.ToBytes();
        Tsd check = Parse(outBytes);
        bool ok = check.Triggers.Count == mod.Triggers.Count && check.Events.Count == mod.Events.Count
            && movedEvents.All(e => check.Events[check.EventIndex(e.Key)].SequenceEqual(e.Value))
            && movedTriggers.All(t => check.Triggers[check.TriggerIndex(t.Key)].SequenceEqual(t.Value))
            && eventOnlySources.All(id => check.Events[check.EventIndex(id)].SequenceEqual(vanilla(van.Events, id)))
            && triggerOnlySources.All(id => check.Triggers[check.TriggerIndex(id)].SequenceEqual(vanilla(van.Triggers, id)));
        if (!ok) throw new InvalidDataException("Lobby event writer check failed");
        File.WriteAllBytes(path, outBytes);
        log.Add("Lobby events: " + movedEvents.Count + " events and " + movedTriggers.Count + " triggers Revamp numbers as in 1.25.02 " +
                "moved to their " + build + " numbers, " + newTriggersFixed + " of its new triggers repointed (fixes Future Gohan's lessons).");
    }

    // After Revamp's uninstall: its uninstaller restores the 1.25 numbers it
    // wrote, not the new ones Port moved its entries to.
    public static void Restore(string game, string build, List<string> log)
    {
        string path = LoosePath(game);
        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        if (!File.Exists(path) || !cpk.Exists(Name)) return;
        Tables m = ByBuild[build];
        Tsd van = Parse(cpk.Read(Name)), mod = Parse(File.ReadAllBytes(path));
        int restored = 0;
        foreach (int id in Column(EventMoves, 0).Union(Column(EventMoves, 1)))
        {
            int at = mod.EventIndex(id), vat = van.EventIndex(id);
            if (at >= 0 && vat >= 0 && !mod.Events[at].SequenceEqual(van.Events[vat])) { mod.Events[at] = van.Events[vat]; restored++; }
        }
        foreach (int id in Column(m.Trigger, 0).Union(Column(m.Trigger, 1)))
        {
            int at = mod.TriggerIndex(id), vat = van.TriggerIndex(id);
            if (at >= 0 && vat >= 0 && !mod.Triggers[at].SequenceEqual(van.Triggers[vat])) { mod.Triggers[at] = van.Triggers[vat]; restored++; }
        }
        if (restored == 0) return;
        File.WriteAllBytes(path, mod.ToBytes());
        log.Add("Lobby events: " + restored + " entries moved for Revamp put back to the game's.");
    }
}

// ------------------------------------------------------------------ HCI

static class CharaImage
{
    const string Dir = @"ui/CharaImage/";
    const string RevampTable = "chara_image_TU21.hci";  // what Revamp 5.1.1 writes

    static List<byte[]> Read(byte[] d)
    {
        if (d.Length < 16 || Encoding.ASCII.GetString(d, 0, 4) != "#CHI") throw new InvalidDataException("not a character image (.hci) file");
        int n = BitConverter.ToInt32(d, 8), off = BitConverter.ToInt32(d, 12);
        var list = new List<byte[]>(n);
        for (int i = 0; i < n; i++) { var e = new byte[16]; Buffer.BlockCopy(d, off + i * 16, e, 0, 16); list.Add(e); }
        return list;
    }

    static byte[] Write(byte[] header, List<byte[]> entries)
    {
        var o = new byte[16 + entries.Count * 16];
        Buffer.BlockCopy(header, 0, o, 0, 8);
        BitConverter.GetBytes(entries.Count).CopyTo(o, 8);
        BitConverter.GetBytes(16).CopyTo(o, 12);
        for (int i = 0; i < entries.Count; i++) Buffer.BlockCopy(entries[i], 0, o, 16 + i * 16, 16);
        return o;
    }

    // chara, costume, state1, state2
    static string Key(byte[] e) { return BitConverter.ToString(e, 0, 8); }

    // Returns the created file's path relative to the game folder, or null.
    public static string Update(string game, List<string> log)
    {
        string installed = Path.Combine(game, "data", Dir.Replace('/', '\\') + RevampTable);
        if (!File.Exists(installed)) { log.Add("No Revamp character image table - skipped."); return null; }

        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        // Newest TU table the game has: TU22, TU22_5, TU23, ...
        string newest = null;
        for (int n = 21; n < 60; n++)
            foreach (string s in new[] { "TU" + n, "TU" + n + "_5" })
                if (cpk.Exists("data/" + Dir + "chara_image_" + s + ".hci")) newest = "chara_image_" + s + ".hci";
        if (newest == null || newest == RevampTable) { log.Add("Character image table: game still uses " + RevampTable + "."); return null; }

        byte[] vanOldBytes = cpk.Read("data/" + Dir + RevampTable);
        byte[] vanNewBytes = cpk.Read("data/" + Dir + newest);
        var vanOld = new HashSet<string>(Read(vanOldBytes).Select(Key));
        var vanNew = Read(vanNewBytes);
        var merged = Read(File.ReadAllBytes(installed));
        var have = new HashSet<string>(merged.Select(Key));

        // Insert each entry the update added after its vanilla predecessor
        // (entry order matters: 0xFFFF costume/state entries are fallbacks).
        int added = 0;
        for (int i = 0; i < vanNew.Count; i++)
        {
            string k = Key(vanNew[i]);
            if (vanOld.Contains(k) || have.Contains(k)) continue;
            int at = 0;
            for (int j = i - 1; j >= 0; j--)
            {
                string pk = Key(vanNew[j]);
                int idx = merged.FindIndex(e => Key(e) == pk);
                if (idx >= 0) { at = idx + 1; break; }
            }
            merged.Insert(at, vanNew[i]);
            have.Add(k);
            added++;
        }

        string rel = @"data\" + Dir.Replace('/', '\\') + newest;
        byte[] outBytes = Write(vanNewBytes, merged);
        string path = Path.Combine(game, rel);
        if (!File.Exists(path) || !File.ReadAllBytes(path).SequenceEqual(outBytes))
            File.WriteAllBytes(path, outBytes);
        log.Add("Character image table " + newest + ": Revamp's " + (merged.Count - added) + " entries + " + added + " added by the update.");
        return rel;
    }
}

// ------------------------------------------------------------------ IGGYTEX

// Iggy menus: a .iggy (Flash movie) refers to the textures in its .iggytex /
// .iggyted files by id. 1.26 renumbered the textures of several menus. Where
// Revamp ships one side and the game supplies the other, the two no longer
// match and the menu shows garbage:
//  * Revamp's textures under the game's new .iggy: the game's file with each
//    of Revamp's textures moved to its new id (Revamp's file is replaced).
//  * Revamp's (1.25.02-based) .iggy over the game's new textures: a loose copy
//    of the game's textures numbered the old way (created by the bridge).
// Which numbering a loose .iggy uses is read from its id table: xv2ins 4.7
// installs CHARASELE, STAGESELE, ITEMLIST_DETAILS and POPUP_PAUSE .iggy files
// rebuilt for the current game, which count as the game's side.
// iggymaps_<build>.txt (tools\iggypairs.py): per texture file, its .iggy, the
// 1.25.02 and that build's ids and old id -> new id for the textures that match.
//
// File: "oggytex\0", u32 count, u32, then per texture {u16 id, u16, size,
// offset, u32} (size/offset with bit 31 set), each a DDS at a 128-byte
// aligned offset, nothing after the last. An entry with size 0 and offset 0
// is a reference to a texture in another file, named by its high u16
// (0xFFFF for textures held here).
static class IggyTex
{
    class Tex { public uint Id, SizeHi, OffHi, Extra; public byte[] Data; public int Key { get { return (int)(Id & 0xFFFF); } } }

    static List<Tex> Read(byte[] d, out byte[] head)
    {
        if (d.Length < 16 || Encoding.ASCII.GetString(d, 0, 8) != "oggytex\0") throw new InvalidDataException("not an Iggy texture file");
        head = new byte[16]; Buffer.BlockCopy(d, 0, head, 0, 16);
        int n = BitConverter.ToInt32(d, 8);
        var list = new List<Tex>(n);
        for (int i = 0; i < n; i++)
        {
            int e = 16 + 16 * i;
            uint size = BitConverter.ToUInt32(d, e + 4), off = BitConverter.ToUInt32(d, e + 8);
            var t = new Tex { Id = BitConverter.ToUInt32(d, e), SizeHi = size & 0x80000000, OffHi = off & 0x80000000, Extra = BitConverter.ToUInt32(d, e + 12) };
            t.Data = new byte[size & 0x7FFFFFFF];
            Buffer.BlockCopy(d, (int)(off & 0x7FFFFFFF), t.Data, 0, t.Data.Length);
            list.Add(t);
        }
        return list;
    }

    static int Align(int v) { return (v + 127) & ~127; }

    static byte[] Write(byte[] head, List<Tex> list)
    {
        int pos = Align(16 + 16 * list.Count);
        var offs = new int[list.Count];
        for (int i = 0; i < list.Count; i++)
        {
            if (list[i].Data.Length == 0) continue;  // reference: offset 0
            pos = Align(pos); offs[i] = pos; pos += list[i].Data.Length;
        }
        var o = new byte[pos];
        Buffer.BlockCopy(head, 0, o, 0, 16);
        BitConverter.GetBytes(list.Count).CopyTo(o, 8);
        for (int i = 0; i < list.Count; i++)
        {
            int e = 16 + 16 * i;
            BitConverter.GetBytes(list[i].Id).CopyTo(o, e);
            BitConverter.GetBytes((uint)list[i].Data.Length | list[i].SizeHi).CopyTo(o, e + 4);
            BitConverter.GetBytes((uint)offs[i] | list[i].OffHi).CopyTo(o, e + 8);
            BitConverter.GetBytes(list[i].Extra).CopyTo(o, e + 12);
            Buffer.BlockCopy(list[i].Data, 0, o, offs[i], list[i].Data.Length);
        }
        return o;
    }

    // An .iggy's table of named characters: 24-byte records {u64 name offset
    // (increasing), u64 character id (decreasing), u64 0}. Texture ids are
    // character ids, so an update that renumbers textures changes this table.
    // Returns the ids of the longest such run (tools\iggypairs.py idtable).
    static List<long> IdTable(byte[] b)
    {
        var best = new List<long>();
        for (int start = 0; start < 24; start += 8)
        {
            var run = new List<long>();
            long pa = -1, pi = -1;
            for (int p = start; p < b.Length - 24; p += 24)
            {
                long a = BitConverter.ToInt64(b, p), i = BitConverter.ToInt64(b, p + 8), z = BitConverter.ToInt64(b, p + 16);
                bool rec = z == 0 && a > 0 && a < b.Length && i >= 0 && i < 0x10000;
                if (rec && (pa < 0 || (a > pa && i < pi))) { run.Add(i); pa = a; pi = i; continue; }
                if (run.Count > best.Count) best = run;
                run = new List<long>();
                if (rec) { run.Add(i); pa = a; pi = i; } else { pa = -1; pi = -1; }
            }
            if (run.Count > best.Count) best = run;
        }
        return best;
    }

    static List<int> Ids(string csv) { return csv.Split(',').Select(int.Parse).ToList(); }
    static string Rel(string cpkName) { return @"data\" + cpkName.Substring(5).Replace('/', '\\'); }

    // Adds the files it creates to `created`.
    public static void Port(string game, string build, string bridgeDir, List<string> created, List<string> log)
    {
        string mapName = "iggymaps_" + build + ".txt";
        string mapFile = Path.Combine(bridgeDir, mapName);
        if (!File.Exists(mapFile)) throw new FileNotFoundException(mapName + " is missing next to the bridge");
        var cpk = new CpkSet(Path.Combine(game, "cpk"));
        int toNew = 0, toOld = 0, current = 0, removed = 0;
        var odd = new List<string>();
        var keep = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var swfOld = new Dictionary<string, bool>();
        foreach (string line in File.ReadAllLines(mapFile))
        {
            if (line.Length == 0 || line[0] == '#') continue;
            var f = line.Split('|');
            string texName = f[0], swfName = f[1];
            var oldIds = Ids(f[2]); var newIds = Ids(f[3]);
            var map = f[4].Split(',').Select(p => p.Split(':')).ToDictionary(p => int.Parse(p[0]), p => int.Parse(p[1]));

            string texPath = Path.Combine(game, Rel(texName));
            bool ours = created.Any(c => string.Equals(c, Rel(texName), StringComparison.OrdinalIgnoreCase));
            bool texLoose = File.Exists(texPath) && !ours;
            // A loose .iggy counts as old only if numbered unlike the game's:
            // xv2ins installs its own CHARASELE/STAGESELE/... rebuilt for the new game.
            bool swfLoose;
            if (!swfOld.TryGetValue(swfName, out swfLoose))
            {
                string swfPath = Path.Combine(game, Rel(swfName));
                swfLoose = File.Exists(swfPath) && cpk.Exists(swfName) &&
                           !IdTable(File.ReadAllBytes(swfPath)).SequenceEqual(IdTable(cpk.Read(swfName)));
                swfOld[swfName] = swfLoose;
            }
            if (texLoose == swfLoose) continue;  // both Revamp's or both the game's: they match
            if (!cpk.Exists(texName)) { odd.Add(texName + " (not in the game)"); continue; }
            byte[] vanHead, vanBytes = cpk.Read(texName);
            var van = Read(vanBytes, out vanHead);
            if (!Write(vanHead, van).SequenceEqual(vanBytes)) throw new InvalidDataException("Iggy texture writer does not reproduce " + texName);
            if (!van.Select(t => t.Key).SequenceEqual(newIds)) { odd.Add(texName + " (the game's file is not the " + build + " one)"); continue; }

            if (texLoose)
            {
                // Revamp's textures (old ids) -> the game's layout
                byte[] head;
                var mod = Read(File.ReadAllBytes(texPath), out head);
                var ids = mod.Select(t => t.Key).ToList();
                if (ids.SequenceEqual(newIds)) { current++; continue; }
                if (!ids.SequenceEqual(oldIds)) { odd.Add(texName + " (unknown layout)"); continue; }
                // references (no data) stay the game's
                var byNew = mod.Where(t => map.ContainsKey(t.Key) && t.Data.Length > 0).ToDictionary(t => map[t.Key], t => t.Data);
                foreach (var t in van) { byte[] d; if (t.Data.Length > 0 && byNew.TryGetValue(t.Key, out d)) t.Data = d; }
                File.WriteAllBytes(texPath, Write(vanHead, van));
                toNew++;
            }
            else
            {
                // Revamp's old .iggy -> a loose copy of the game's textures with the old ids
                if (oldIds.Any(i => !map.ContainsKey(i))) { odd.Add(texName + " (has textures the update redrew)"); continue; }
                var byId = van.ToDictionary(t => t.Key);
                var list = oldIds.Select(i =>
                {
                    var v = byId[map[i]];
                    return new Tex { Id = (v.Id & 0xFFFF0000) | (uint)i, SizeHi = v.SizeHi, OffHi = v.OffHi, Extra = v.Extra, Data = v.Data };
                }).ToList();
                byte[] outBytes = Write(vanHead, list);
                string rel = Rel(cpk.Name(texName));  // the game's own spelling
                string path = Path.Combine(game, rel);
                if (File.Exists(path) && File.ReadAllBytes(path).SequenceEqual(outBytes)) current++;
                else { File.WriteAllBytes(path, outBytes); toOld++; }
                if (!ours) created.Add(rel);
                keep.Add(rel);
            }
        }
        // copies made earlier that no menu needs any more (the .iggy changed)
        foreach (string rel in created.Where(c => Regex.IsMatch(c, @"\\iggy\\[^\\]+\.iggyte[xd]$", RegexOptions.IgnoreCase) && !keep.Contains(c)).ToList())
        {
            string p = Path.Combine(game, rel);
            if (File.Exists(p)) { File.Delete(p); removed++; }
            created.Remove(rel);
        }
        log.Add("Menu textures: " + toNew + " of Revamp's moved to " + build + "'s numbering, " + toOld +
                " of the game's numbered for Revamp's menus" + (current > 0 ? ", " + current + " already current" : "") +
                (removed > 0 ? ", " + removed + " earlier copies removed" : "") + ".");
        if (odd.Count > 0) log.Add("  Left unchanged: " + string.Join(", ", odd));
    }
}

// ------------------------------------------------------------------ CPK

// @UTF table with the byte position of every row value, so fixed-width
// fields can be changed in place.
class Utf
{
    public byte[] Plain;
    public bool Encrypted;
    public class Col { public string Name; public int Storage, Type; }
    public List<Col> Cols = new List<Col>();
    public List<Dictionary<string, int>> RowPos = new List<Dictionary<string, int>>();
    int strOff, dataOff;

    static readonly int[] Size = { 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 4, 8 };

    static byte[] Crypt(byte[] d)
    {
        var o = new byte[d.Length];
        uint m = 0x655F, t = 0x4115;
        for (int i = 0; i < d.Length; i++) { o[i] = (byte)(d[i] ^ (m & 0xFF)); m *= t; }
        return o;
    }

    public Utf(byte[] data)
    {
        Plain = data;
        if (Encoding.ASCII.GetString(data, 0, 4) != "@UTF") { Plain = Crypt(data); Encrypted = true; }
        if (Encoding.ASCII.GetString(Plain, 0, 4) != "@UTF") throw new InvalidDataException("not an @UTF table");
        int rowsOff = BE32(8) + 8; strOff = BE32(12) + 8; dataOff = BE32(16) + 8;
        int ncols = BE16(24), rowLen = BE16(26), nrows = BE32(28);
        int pos = 32;
        for (int c = 0; c < ncols; c++)
        {
            int flags = Plain[pos];
            var col = new Col { Name = Str(BE32(pos + 1)), Storage = flags & 0xF0, Type = flags & 0x0F };
            pos += 5;
            if (col.Storage == 0x30 || col.Storage == 0x70) pos += Size[col.Type];
            Cols.Add(col);
        }
        for (int r = 0; r < nrows; r++)
        {
            var d = new Dictionary<string, int>();
            int p = rowsOff + r * rowLen;
            foreach (var col in Cols)
                if (col.Storage == 0x50) { d[col.Name] = p; p += Size[col.Type]; }
            RowPos.Add(d);
        }
    }

    int BE16(int o) { return (Plain[o] << 8) | Plain[o + 1]; }
    int BE32(int o) { return (int)((uint)Plain[o] << 24 | (uint)Plain[o + 1] << 16 | (uint)Plain[o + 2] << 8 | Plain[o + 3]); }
    string Str(int off) { int s = strOff + off, e = Array.IndexOf(Plain, (byte)0, s); return Encoding.ASCII.GetString(Plain, s, e - s); }

    Col C(string name) { return Cols.First(c => c.Name == name); }

    public bool Has(int row, string name) { return RowPos[row].ContainsKey(name); }

    public ulong GetU(int row, string name)
    {
        int p = RowPos[row][name]; int n = Size[C(name).Type]; ulong v = 0;
        for (int i = 0; i < n; i++) v = v << 8 | Plain[p + i];
        return v;
    }

    public string GetS(int row, string name) { return Str(BE32(RowPos[row][name])); }

    public void SetU(int row, string name, ulong v)
    {
        var col = C(name);
        if (col.Type > 7) throw new InvalidOperationException(name + " is not an integer column");
        if (!RowPos[row].ContainsKey(name)) throw new InvalidOperationException(name + " is not stored per row");
        int p = RowPos[row][name]; int n = Size[col.Type];
        if (n < 8 && v >> (8 * n) != 0) throw new OverflowException(name + " does not fit");
        for (int i = n - 1; i >= 0; i--) { Plain[p + i] = (byte)v; v >>= 8; }
    }

    public byte[] Bytes() { return Encrypted ? Crypt(Plain) : (byte[])Plain.Clone(); }
    public int Rows { get { return RowPos.Count; } }
}

class Packet
{
    public byte[] Head;  // 16 bytes: tag, 4 bytes, u64 size (LE)
    public Utf Table;
    public static Packet Read(Stream s, long off)
    {
        s.Position = off;
        var p = new Packet { Head = ReadN(s, 16) };
        long size = BitConverter.ToInt64(p.Head, 8);
        p.Table = new Utf(ReadN(s, (int)size));
        return p;
    }
    public byte[] Bytes() { var t = Table.Bytes(); var o = new byte[16 + t.Length]; Head.CopyTo(o, 0); t.CopyTo(o, 16); return o; }
    public static byte[] ReadN(Stream s, int n)
    {
        var b = new byte[n]; int got = 0;
        while (got < n) { int r = s.Read(b, got, n - got); if (r <= 0) throw new EndOfStreamException(); got += r; }
        return b;
    }
}

class CpkFile
{
    public string Path;
    public Packet Header, Toc;
    public long DataBase;  // absolute = FileOffset + DataBase
    public Dictionary<string, int> Row = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

    public CpkFile(string path)
    {
        Path = path;
        using (var s = File.OpenRead(path))
        {
            Header = Packet.Read(s, 0);
            long toc = (long)Header.Table.GetU(0, "TocOffset"), content = (long)Header.Table.GetU(0, "ContentOffset");
            DataBase = toc < content ? toc : content;
            Toc = Packet.Read(s, toc);
        }
        for (int r = 0; r < Toc.Table.Rows; r++)
        {
            string dir = Toc.Table.GetS(r, "DirName"), name = Toc.Table.GetS(r, "FileName");
            Row[(dir.Length > 0 ? dir + "/" : "") + name] = r;
        }
    }

    public byte[] Stored(Stream s, string name)
    {
        int r = Row[name];
        s.Position = (long)Toc.Table.GetU(r, "FileOffset") + DataBase;
        return Packet.ReadN(s, (int)Toc.Table.GetU(r, "FileSize"));
    }
}

class CpkSet
{
    readonly List<CpkFile> cpks = new List<CpkFile>();
    public CpkSet(string dir)
    {
        // same order as the installer: data2, data1, data0, data, data_d4_5_xv1
        foreach (string n in new[] { "data2.cpk", "data1.cpk", "data0.cpk", "data.cpk", "data_d4_5_xv1.cpk" })
            if (File.Exists(System.IO.Path.Combine(dir, n))) cpks.Add(new CpkFile(System.IO.Path.Combine(dir, n)));
    }
    public bool Exists(string name) { return cpks.Any(c => c.Row.ContainsKey(name)); }
    // the name as the archive spells it
    public string Name(string name) { return cpks.First(x => x.Row.ContainsKey(name)).Row.Keys.First(k => string.Equals(k, name, StringComparison.OrdinalIgnoreCase)); }
    public byte[] Read(string name)
    {
        var c = cpks.First(x => x.Row.ContainsKey(name));
        byte[] d;
        using (var s = File.OpenRead(c.Path)) d = c.Stored(s, name);
        int r = c.Row[name];
        ulong ext = c.Toc.Table.GetU(r, "ExtractSize");
        return (ulong)d.Length == ext ? d : Crilayla.Decompress(d);
    }
}

static class Cpk
{
    static long Align(long v, long a) { return (v + a - 1) / a * a; }

    public static string RebuildRvp(string rvpPath, string data2Path)
    {
        var rvp = new CpkFile(rvpPath);
        var d2 = new CpkFile(data2Path);

        // Already current? (compare the online config's stored bytes)
        using (var a = File.OpenRead(rvpPath))
        using (var b = File.OpenRead(data2Path))
            if (rvp.Row.ContainsKey("data2/config.dat") && d2.Row.ContainsKey("data2/config.dat") &&
                rvp.Stored(a, "data2/config.dat").SequenceEqual(d2.Stored(b, "data2/config.dat")))
                return "rvp.cpk already has this game version's data2 files.";

        var missing = d2.Row.Keys.Where(k => !rvp.Row.ContainsKey(k)).ToList();
        if (missing.Count > 0)
            throw new InvalidDataException("data2.cpk has files rvp.cpk has no slot for: " + string.Join(", ", missing));

        long align = (long)rvp.Header.Table.GetU(0, "Align");
        long content = (long)rvp.Header.Table.GetU(0, "ContentOffset");
        var order = Enumerable.Range(0, rvp.Toc.Table.Rows).OrderBy(r => rvp.Toc.Table.GetU(r, "FileOffset")).ToList();
        string tmp = rvpPath + ".bridge-tmp";
        ulong sumStored = 0, sumExtract = 0;
        int fromData2 = 0;

        using (var src = File.OpenRead(rvpPath))
        using (var d2s = File.OpenRead(data2Path))
        using (var dst = new FileStream(tmp, FileMode.Create, FileAccess.Write))
        {
            dst.Write(new byte[content], 0, (int)content);  // header written last
            long pos = content;
            var buf = new byte[1 << 20];
            foreach (int r in order)
            {
                string name = rvp.Row.First(kv => kv.Value == r).Key;
                pos = Align(pos, align);
                dst.Position = pos;
                ulong stored, extract;
                if (d2.Row.ContainsKey(name))
                {
                    byte[] data = d2.Stored(d2s, name);
                    dst.Write(data, 0, data.Length);
                    stored = (ulong)data.Length;
                    extract = d2.Toc.Table.GetU(d2.Row[name], "ExtractSize");
                    fromData2++;
                }
                else
                {
                    // Revamp's own file: stream it across
                    src.Position = (long)rvp.Toc.Table.GetU(r, "FileOffset") + rvp.DataBase;
                    stored = rvp.Toc.Table.GetU(r, "FileSize");
                    extract = rvp.Toc.Table.GetU(r, "ExtractSize");
                    long left = (long)stored;
                    while (left > 0)
                    {
                        int n = src.Read(buf, 0, (int)Math.Min(buf.Length, left));
                        if (n <= 0) throw new EndOfStreamException();
                        dst.Write(buf, 0, n); left -= n;
                    }
                }
                rvp.Toc.Table.SetU(r, "FileOffset", (ulong)(pos - rvp.DataBase));
                rvp.Toc.Table.SetU(r, "FileSize", stored);
                rvp.Toc.Table.SetU(r, "ExtractSize", extract);
                sumStored += stored; sumExtract += extract;
                pos += (long)stored;
            }

            long tocPos = Align(pos, align);
            dst.Position = tocPos;
            byte[] toc = rvp.Toc.Bytes();
            dst.Write(toc, 0, toc.Length);
            pos = tocPos + toc.Length;
            long etocPos = -1;
            if (rvp.Header.Table.Has(0, "EtocOffset") && rvp.Header.Table.GetU(0, "EtocOffset") != 0)
            {
                // copied unchanged (per-directory update times)
                var etoc = Packet.Read(src, (long)rvp.Header.Table.GetU(0, "EtocOffset")).Bytes();
                etocPos = Align(pos, align);
                dst.Position = etocPos;
                dst.Write(etoc, 0, etoc.Length);
                pos = etocPos + etoc.Length;
            }
            long end = Align(pos, align);
            if (end > pos) { dst.Position = end - 1; dst.WriteByte(0); }

            var h = rvp.Header.Table;
            h.SetU(0, "TocOffset", (ulong)tocPos);
            if (etocPos >= 0) h.SetU(0, "EtocOffset", (ulong)etocPos);
            // Informational totals (CpkMaker stores packed/data this way round).
            if (h.Has(0, "EnabledPackedSize")) h.SetU(0, "EnabledPackedSize", sumExtract);
            if (h.Has(0, "EnabledDataSize")) h.SetU(0, "EnabledDataSize", sumStored);
            byte[] head = rvp.Header.Bytes();
            if (head.Length > content) throw new InvalidDataException("CPK header larger than its slot");
            dst.Position = 0;
            dst.Write(head, 0, head.Length);
        }

        Verify(tmp, rvpPath, data2Path);
        File.Delete(rvpPath);
        File.Move(tmp, rvpPath);
        return "rvp.cpk rebuilt: " + fromData2 + " files from this game's data2.cpk, " + (rvp.Toc.Table.Rows - fromData2) + " of Revamp's own.";
    }

    // Read the new archive back: every file must equal its source.
    static void Verify(string built, string oldRvp, string data2)
    {
        var n = new CpkFile(built);
        var o = new CpkFile(oldRvp);
        var d = new CpkFile(data2);
        using (var ns = File.OpenRead(built))
        using (var os = File.OpenRead(oldRvp))
        using (var ds = File.OpenRead(data2))
            foreach (var kv in n.Row)
            {
                bool fromD2 = d.Row.ContainsKey(kv.Key);
                var srcFile = fromD2 ? d : o; var srcStream = fromD2 ? (Stream)ds : os;
                long size = (long)n.Toc.Table.GetU(kv.Value, "FileSize");
                if (size != (long)srcFile.Toc.Table.GetU(srcFile.Row[kv.Key], "FileSize"))
                    throw new InvalidDataException("rvp.cpk rebuild check failed (size): " + kv.Key);
                if (size > 64 << 20)
                {
                    // large (the intro movie): compare the first and last MB
                    foreach (long at in new[] { 0L, size - (1 << 20) })
                    {
                        ns.Position = (long)n.Toc.Table.GetU(kv.Value, "FileOffset") + n.DataBase + at;
                        srcStream.Position = (long)srcFile.Toc.Table.GetU(srcFile.Row[kv.Key], "FileOffset") + srcFile.DataBase + at;
                        if (!Packet.ReadN(ns, 1 << 20).SequenceEqual(Packet.ReadN(srcStream, 1 << 20)))
                            throw new InvalidDataException("rvp.cpk rebuild check failed: " + kv.Key);
                    }
                }
                else if (!n.Stored(ns, kv.Key).SequenceEqual(srcFile.Stored(srcStream, kv.Key)))
                    throw new InvalidDataException("rvp.cpk rebuild check failed: " + kv.Key);
            }
    }
}

// CRILAYLA decompression (only needed to read vanilla tables that are stored
// compressed). Port of CriPakTools' DecompressCRILAYLA.
static class Crilayla
{
    public static byte[] Decompress(byte[] input)
    {
        int usize = BitConverter.ToInt32(input, 8), hdrOff = BitConverter.ToInt32(input, 12);
        byte[] result = new byte[usize + 0x100];
        Buffer.BlockCopy(input, hdrOff + 0x10, result, 0, 0x100);
        int inputEnd = input.Length - 0x100 - 1, inputOffset = inputEnd, outputEnd = 0x100 + usize - 1;
        byte bitPool = 0; int bitsLeft = 0, bytesOutput = 0;
        int[] vleLens = { 2, 3, 5, 8 };
        Func<int, ushort> get = n =>
        {
            ushort outBits = 0; int produced = 0;
            while (produced < n)
            {
                if (bitsLeft == 0) { bitPool = input[inputOffset]; bitsLeft = 8; inputOffset--; }
                int take = Math.Min(bitsLeft, n - produced);
                outBits <<= take;
                outBits |= (ushort)((ushort)(bitPool >> (bitsLeft - take)) & ((1 << take) - 1));
                bitsLeft -= take; produced += take;
            }
            return outBits;
        };
        while (bytesOutput < usize)
        {
            if (get(1) > 0)
            {
                int backref = outputEnd - bytesOutput + get(13) + 3, len = 3, lvl;
                for (lvl = 0; lvl < vleLens.Length; lvl++)
                {
                    int v = get(vleLens[lvl]); len += v;
                    if (v != (1 << vleLens[lvl]) - 1) break;
                }
                if (lvl == vleLens.Length) { int v; do { v = get(8); len += v; } while (v == 255); }
                for (int i = 0; i < len; i++) { result[outputEnd - bytesOutput] = result[backref--]; bytesOutput++; }
            }
            else { result[outputEnd - bytesOutput] = (byte)get(8); bytesOutput++; }
        }
        return result;
    }
}
