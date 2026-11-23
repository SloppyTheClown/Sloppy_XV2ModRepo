// Release mode of the bridge ("Revamp Bridge.exe" in the release zip).
//
// The release carries no file of Revamp's. Two parts of the bridge are made
// from Revamp's own files: lib\ (Revamp's Xv2CoreLib, rebuilt with current
// file readers, plus the libraries its installer embeds) and epatches\
// (Revamp's exe patches, ported to each game build). The release exe carries
// binary patches (bsdiff, made by src\make_patches.py) from Revamp's files to
// the bridge's, and rebuilds them here from the player's own download:
//
//   1. find the Revamp download next to this exe: that zip already extracted
//      (its Installer folder, used as it is), else "Revamp Xenoverse 2 -
//      5.1.1.zip", whose Installer folder is extracted into
//      "Revamp Bridge Files\Installer" (once; about 5 GB)
//   2. rebuild lib\, epatches\ and the texture maps in "Revamp Bridge Files"
//      as patches\manifest.txt lists them; every result is checked by SHA-256
//   3. copy this exe there as "Revamp Xenoverse 2 Installer.exe" (the
//      installer opens "<process name>.installinfo", so the process needs that
//      name) and run it - from then on it is the bridge's normal mode.
//
//   "Revamp Bridge.exe" [--fix-only] [--game <game folder>]
//   "Revamp Bridge.exe" --prepare-only [--zip <Revamp zip>]   (steps 1-2 only, no window; for tests)
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;

static class ReleaseSetup
{
    public const string ZipName = "Revamp Xenoverse 2 - 5.1.1.zip";
    const string ExtractedName = "Revamp Xenoverse 2 - 5.1.1";   // the zip's own name, if the player extracted it
    const string WorkName = "Revamp Bridge Files";
    const string ManifestResource = "patches/manifest.txt";

    // Only the release build embeds the patch set.
    public static bool IsRelease
    {
        get { return Assembly.GetExecutingAssembly().GetManifestResourceInfo(ManifestResource) != null; }
    }

    public static int Run(string[] args)
    {
        string here = AppDomain.CurrentDomain.BaseDirectory;
        string zip = null;
        bool prepareOnly = false;
        var passOn = new List<string>();
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--zip" && i + 1 < args.Length) zip = args[++i];
            else if (args[i] == "--prepare-only") prepareOnly = true;
            else passOn.Add(args[i]);
        }

        string work = Path.Combine(here, WorkName);
        // An extracted Revamp download wins over the zip: no 5 GB extraction.
        string extracted = zip == null ? FindExtracted(here) : null;
        if (zip == null && extracted == null) zip = FindZip(here);
        string installerDir;
        if (extracted != null) installerDir = extracted;   // used as it is, never written to
        else if (zip != null) installerDir = Path.Combine(work, "Installer");
        else
        {
            RevampBridge.Fail("\"" + ZipName + "\" was not found.\n\nPut the Revamp download (the zip, or its extracted contents) in this folder:\n" +
                              here + "\n\nthen run Revamp Bridge again.");
            return 1;
        }

        if (prepareOnly)
        {
            // Test mode: no window, no message box - result in the log and exit code.
            var log = new List<string>();
            int code = 0;
            try { Prepare(zip, installerDir, work, null, log); log.Add("OK"); }
            catch (Exception ex) { log.Add("FAILED: " + ex.Message); code = 1; }
            Directory.CreateDirectory(work);
            File.WriteAllLines(Path.Combine(work, "setup.log"), log);
            return code;
        }

        if (!PrepareWithWindow(zip, installerDir, work)) return 1;

        // Run the bridge under the installer's name (see the top of this file).
        string self = Assembly.GetExecutingAssembly().Location;
        string runner = Path.Combine(work, RevampBridge.InstallerName + ".exe");
        if (!File.Exists(runner) || !File.ReadAllBytes(runner).SequenceEqual(File.ReadAllBytes(self)))
            File.Copy(self, runner, true);

        var cmd = new StringBuilder("--revamp " + Quote(installerDir));
        foreach (string a in passOn) cmd.Append(' ').Append(Quote(a));
        var psi = new ProcessStartInfo(runner, cmd.ToString()) { UseShellExecute = false, WorkingDirectory = work };
        using (Process p = Process.Start(psi))
        {
            p.WaitForExit();
            return p.ExitCode;
        }
    }

    static string Quote(string s) { return "\"" + s.TrimEnd('\\') + "\""; }

    // The zip by its own name, else the only "Revamp Xenoverse 2*5.1.1*.zip"
    // (a browser may have added " (1)").
    static string FindZip(string dir)
    {
        string exact = Path.Combine(dir, ZipName);
        if (File.Exists(exact)) return exact;
        string[] found = Directory.GetFiles(dir, "Revamp Xenoverse 2*5.1.1*.zip");
        return found.Length == 1 ? found[0] : null;
    }

    // The Revamp zip already extracted next to this exe: "Extract Here" gives
    // .\Installer, "Extract to <zip name>\" gives .\<any folder>\Installer.
    // "Revamp Bridge Files" is skipped - that is our own copy from the zip.
    static string FindExtracted(string dir)
    {
        string direct = Path.Combine(dir, "Installer");
        if (HasInstaller(direct)) return direct;
        string named = Path.Combine(dir, ExtractedName, "Installer");
        if (HasInstaller(named)) return named;
        foreach (string sub in Directory.GetDirectories(dir))
        {
            if (Path.GetFileName(sub).Equals(WorkName, StringComparison.OrdinalIgnoreCase)) continue;
            string inside = Path.Combine(sub, "Installer");
            if (HasInstaller(inside)) return inside;
        }
        return null;
    }

    static bool HasInstaller(string dir)
    {
        return File.Exists(Path.Combine(dir, RevampBridge.InstallerName + ".exe")) &&
               File.Exists(Path.Combine(dir, RevampBridge.InstallerName + ".installinfo"));
    }

    // Runs Prepare on a worker thread behind a small progress window.
    static bool PrepareWithWindow(string zip, string installerDir, string work)
    {
        var text = new TextBlock { Text = "Starting...", TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 10) };
        var bar = new ProgressBar { Height = 18, Minimum = 0, Maximum = 1 };
        var panel = new StackPanel { Margin = new Thickness(16) };
        panel.Children.Add(text);
        panel.Children.Add(bar);
        var win = new Window
        {
            Title = "Revamp Bridge - preparing the Revamp installer",
            Width = 480, SizeToContent = SizeToContent.Height, ResizeMode = ResizeMode.NoResize,
            WindowStartupLocation = WindowStartupLocation.CenterScreen, Content = panel
        };
        Exception error = null;
        bool done = false;
        Action<string, double> progress = (msg, frac) => win.Dispatcher.BeginInvoke(new Action(() =>
        {
            text.Text = msg;
            bar.IsIndeterminate = frac < 0;
            if (frac >= 0) bar.Value = frac;
        }));
        win.Loaded += delegate
        {
            var t = new Thread(() =>
            {
                try { Prepare(zip, installerDir, work, progress, new List<string>()); }
                catch (Exception ex) { error = ex; }
                done = true;
                win.Dispatcher.BeginInvoke(new Action(win.Close));
            });
            t.IsBackground = true;
            t.Start();
        };
        win.ShowDialog();
        if (!done) return false;   // closed by the player; a half-written file is redone next time
        if (error != null)
        {
            MessageBox.Show("Revamp Bridge could not prepare the Revamp installer:\n\n" + error.Message,
                            "Revamp Bridge", MessageBoxButton.OK, MessageBoxImage.Error);
            return false;
        }
        return true;
    }

    // --- steps 1 and 2 ---

    static void Prepare(string zip, string installerDir, string work, Action<string, double> progress, List<string> log)
    {
        if (progress == null) progress = delegate { };
        Directory.CreateDirectory(work);
        if (zip != null) Extract(zip, installerDir, progress, log);

        string exe = Path.Combine(installerDir, RevampBridge.InstallerName + ".exe");
        string installinfo = Path.Combine(installerDir, RevampBridge.InstallerName + ".installinfo");
        progress("Building the bridge's files from Revamp's...", -1);

        // Sources, read only when a line needs them.
        Dictionary<string, byte[]> exeRes = null, corelibRes = null;
        ZipArchive info = null;
        try
        {
            foreach (string line in ReadResourceText(ManifestResource).Split('\n'))
            {
                string l = line.Trim();
                if (l.Length == 0 || l.StartsWith("#")) continue;
                // target | source | patch resource or "-" | SHA-256 of the result
                string[] f = l.Split('|');
                string target = Path.Combine(work, f[0]);
                if (File.Exists(target) && Sha256(File.ReadAllBytes(target)) == f[3]) { log.Add("ok      " + f[0]); continue; }

                string kind = f[1].Substring(0, f[1].IndexOf(':')), name = f[1].Substring(kind.Length + 1);
                byte[] data;
                if (kind == "bridge") data = ReadResource(name);
                else if (kind == "installinfo")
                {
                    if (info == null) info = ZipFile.OpenRead(installinfo);
                    ZipArchiveEntry e = info.GetEntry(name);
                    if (e == null) throw new InvalidDataException("Revamp's .installinfo has no " + name + " - is this Revamp 5.1.1?");
                    data = ReadAll(e.Open(), e.Length);
                }
                else
                {
                    if (exeRes == null) exeRes = CosturaResources(File.ReadAllBytes(exe));
                    Dictionary<string, byte[]> res = exeRes;
                    if (kind == "corelib")
                    {
                        if (corelibRes == null) corelibRes = CosturaResources(Need(exeRes, "xv2corelib.dll"));
                        res = corelibRes;
                    }
                    data = Need(res, name);
                }
                if (f[2] != "-") data = ApplyPatch(data, ReadResource(f[2]), f[1]);
                if (Sha256(data) != f[3])
                    throw new InvalidDataException(f[1] + " in your Revamp download is not the one from Revamp 5.1.1 (" + f[0] + " came out different).");
                Directory.CreateDirectory(Path.GetDirectoryName(target));
                File.WriteAllBytes(target, data);
                log.Add("written " + f[0]);
            }
        }
        finally { if (info != null) info.Dispose(); }
    }

    // Extracts the zip's Installer folder (the installer exe and its .installinfo).
    // A file already there at its full size is kept; each file is written under
    // a temporary name first so a cancelled run never leaves a short file.
    static void Extract(string zip, string dest, Action<string, double> progress, List<string> log)
    {
        using (ZipArchive z = ZipFile.OpenRead(zip))
        {
            var todo = z.Entries.Where(e => e.FullName.StartsWith("Installer/", StringComparison.OrdinalIgnoreCase) && e.Name.Length > 0).ToList();
            if (!todo.Any(e => e.Name.Equals(RevampBridge.InstallerName + ".installinfo", StringComparison.OrdinalIgnoreCase)))
                throw new InvalidDataException(Path.GetFileName(zip) + " has no Installer\\" + RevampBridge.InstallerName +
                                               ".installinfo - is it the Revamp Xenoverse 2 5.1.1 download?");
            Directory.CreateDirectory(dest);
            todo = todo.Where(e => { var fi = new FileInfo(Path.Combine(dest, e.Name)); return !fi.Exists || fi.Length != e.Length; }).ToList();
            if (todo.Count == 0) { log.Add("ok      Installer (already extracted)"); return; }

            long total = todo.Sum(e => e.Length), doneBytes = 0;
            long free = FreeSpace(dest);
            if (free >= 0 && free < total + (100L << 20))
                throw new IOException("Not enough disk space: extracting the Revamp installer needs " + Gb(total) + " GB free on the drive of\n" + dest);

            var buf = new byte[1 << 20];
            foreach (ZipArchiveEntry e in todo)
            {
                string target = Path.Combine(dest, e.Name), part = target + ".part";
                // The entry stream can end early without an error (seen once: the 4.9 GB
                // .installinfo stopped at 70 MB, a rerun was fine), so the length is
                // checked and the entry read again from the start.
                const int attempts = 3;
                for (int attempt = 1; ; attempt++)
                {
                    long written = 0, startBytes = doneBytes;
                    using (Stream src = e.Open())
                    using (FileStream dst = File.Create(part))
                    {
                        int n;
                        while ((n = src.Read(buf, 0, buf.Length)) > 0)
                        {
                            dst.Write(buf, 0, n);
                            written += n;
                            doneBytes += n;
                            progress("Extracting the Revamp installer from the zip (once)...\n" + e.Name + "   " +
                                     Gb(doneBytes) + " / " + Gb(total) + " GB", (double)doneBytes / total);
                        }
                    }
                    if (written == e.Length) break;
                    log.Add("short   Installer\\" + e.Name + ": " + written + " of " + e.Length + " bytes, attempt " + attempt);
                    doneBytes = startBytes;
                    if (attempt == attempts)
                    {
                        File.Delete(part);
                        throw new IOException("Could not read Installer\\" + e.Name + " completely from\n" + zip +
                                              "\nThe zip may be damaged or still downloading.");
                    }
                }
                if (File.Exists(target)) File.Delete(target);
                File.Move(part, target);
                log.Add("written Installer\\" + e.Name);
            }
        }
    }

    static string Gb(long bytes) { return (bytes / (double)(1L << 30)).ToString("0.0"); }

    static long FreeSpace(string dir)
    {
        try { return new DriveInfo(Path.GetPathRoot(Path.GetFullPath(dir))).AvailableFreeSpace; }
        catch { return -1; }   // network path etc.: let the write fail if it must
    }

    // Costura (used by LB Mod Installer builds) embeds each dependency as a
    // deflated resource "costura.<name>.dll.compressed".
    static Dictionary<string, byte[]> CosturaResources(byte[] assembly)
    {
        var res = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
        Assembly asm = Assembly.ReflectionOnlyLoad(assembly);
        foreach (string r in asm.GetManifestResourceNames())
        {
            if (!r.StartsWith("costura.") || !r.EndsWith(".dll.compressed")) continue;
            using (var d = new DeflateStream(asm.GetManifestResourceStream(r), CompressionMode.Decompress))
                res[r.Substring(8, r.Length - 8 - 11)] = ReadAll(d, -1);
        }
        return res;
    }

    static byte[] Need(Dictionary<string, byte[]> res, string name)
    {
        byte[] b;
        if (!res.TryGetValue(name, out b)) throw new InvalidDataException("The Revamp installer has no embedded " + name + " - is this Revamp 5.1.1?");
        return b;
    }

    // --- patches ---

    // Applies a bsdiff patch from src\make_patches.py:
    //   "RBPATCH1", int64 old length, int64 new length, SHA-256 old, SHA-256 new,
    //   int64 control/diff/extra lengths, then the three blocks raw-deflated.
    // Control: (x, y, z) triples of bsdiff's sign-magnitude int64 - add x diff
    // bytes to old, copy y extra bytes, move the old position by z.
    static byte[] ApplyPatch(byte[] old, byte[] patch, string what)
    {
        var r = new BinaryReader(new MemoryStream(patch));
        if (Encoding.ASCII.GetString(r.ReadBytes(8)) != "RBPATCH1") throw new InvalidDataException("Damaged patch for " + what + ".");
        long oldLen = r.ReadInt64(), newLen = r.ReadInt64();
        string oldSha = Hex(r.ReadBytes(32)), newSha = Hex(r.ReadBytes(32));
        long ctrlLen = r.ReadInt64(), diffLen = r.ReadInt64(), extraLen = r.ReadInt64();
        if (old.Length != oldLen || Sha256(old) != oldSha)
            throw new InvalidDataException(what + " in your Revamp download is not the one from Revamp 5.1.1.");
        byte[] body;
        using (var d = new DeflateStream(r.BaseStream, CompressionMode.Decompress))
            body = ReadAll(d, ctrlLen + diffLen + extraLen);

        var result = new byte[newLen];
        long newPos = 0, oldPos = 0, ctrl = 0, diff = ctrlLen, extra = ctrlLen + diffLen;
        while (newPos < newLen)
        {
            long x = Offtin(body, ctrl), y = Offtin(body, ctrl + 8), z = Offtin(body, ctrl + 16);
            ctrl += 24;
            for (long i = 0; i < x; i++)
            {
                byte b = body[diff + i];
                long o = oldPos + i;
                if (o >= 0 && o < old.Length) b = (byte)(b + old[o]);
                result[newPos + i] = b;
            }
            diff += x; newPos += x; oldPos += x;
            Array.Copy(body, extra, result, newPos, y);
            extra += y; newPos += y; oldPos += z;
        }
        if (Sha256(result) != newSha) throw new InvalidDataException("Patching " + what + " gave a wrong result.");
        return result;
    }

    static long Offtin(byte[] b, long at)
    {
        long v = BitConverter.ToInt64(b, (int)at) & long.MaxValue;
        return (b[at + 7] & 0x80) != 0 ? -v : v;
    }

    // --- helpers ---

    static string ReadResourceText(string name) { return Encoding.UTF8.GetString(ReadResource(name)); }

    static byte[] ReadResource(string name)
    {
        Stream s = Assembly.GetExecutingAssembly().GetManifestResourceStream(name);
        if (s == null) throw new InvalidDataException("This Revamp Bridge.exe lacks its resource " + name + " - download it again.");
        using (s) return ReadAll(s, s.Length);
    }

    static byte[] ReadAll(Stream s, long length)
    {
        using (s)
        {
            if (length < 0)
            {
                var ms = new MemoryStream();
                s.CopyTo(ms);
                return ms.ToArray();
            }
            var buf = new byte[length];
            long got = 0;
            while (got < length)
            {
                int n = s.Read(buf, (int)got, (int)Math.Min(int.MaxValue, length - got));
                if (n <= 0) throw new EndOfStreamException();
                got += n;
            }
            return buf;
        }
    }

    static string Sha256(byte[] data) { using (var h = SHA256.Create()) return Hex(h.ComputeHash(data)); }

    static string Hex(byte[] b) { return BitConverter.ToString(b).Replace("-", "").ToLowerInvariant(); }
}
