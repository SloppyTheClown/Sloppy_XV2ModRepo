// Runs the unmodified Revamp Xenoverse 2 installer on game 1.26.01 or 1.27.
// (The release exe starts in src\Release.cs, which prepares this mode.)
//
// The Revamp installer (a fork of LB Mod Installer, built for game 1.25.02)
// embeds a fork of Xv2CoreLib that cannot read the item (.idb), quest (.qxd),
// character select (.cst) and XV2P_SLOTS formats of later versions. This
// launcher loads the original installer in-process and serves every assembly
// found in lib\ in place of its embedded copies: Revamp's own Xv2CoreLib,
// rebuilt with current readers for those formats (src\build_lib.ps1), plus
// that library's dependencies. After the installer closes, PostInstall
// applies the fixes that are about the game rather than the installer.
//
// The installer finds "<process name>.installinfo" in the working directory,
// so this exe carries the installer's name and switches to the Revamp
// Installer folder before starting it. Nothing in that folder is modified
// (the installer only writes install_error.txt there if it fails).
//
//   "Revamp Xenoverse 2 Installer.exe" [--revamp <Installer folder>] [--game <game folder>]
//   "Revamp Xenoverse 2 Installer.exe" --fix-only [--game <game folder>]
//
// --revamp defaults to RevampDir= in RevampBridge.ini next to this exe.
// --game sets the game folder the installer uses (it is remembered by the
//   installer itself, in %APPDATA%\LB Mod Installer 3\settings.xml).
// --fix-only applies the post-install fixes without running the installer.
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Windows;
using System.Xml;

static class RevampBridge
{
    internal const string InstallerName = "Revamp Xenoverse 2 Installer";

    static string libDir;
    static readonly Dictionary<string, Assembly> loaded = new Dictionary<string, Assembly>();

    // Serves every assembly in lib\ (Xv2CoreLib and what it depends on);
    // anything else falls through to the installer's own embedded copies.
    static Assembly Resolve(object sender, ResolveEventArgs e)
    {
        string name = new AssemblyName(e.Name).Name.ToLowerInvariant();
        string path = Path.Combine(libDir, name + ".dll");
        lock (loaded)
        {
            Assembly a;
            if (loaded.TryGetValue(name, out a)) return a;
            if (!File.Exists(path)) return null;
            // Loaded from bytes, like Costura does: Xv2CoreLib is flagged
            // 32-bit-required, which only a file load enforces.
            a = Assembly.Load(File.ReadAllBytes(path));
            loaded[name] = a;
            return a;
        }
    }

    internal static void Fail(string msg)
    {
        MessageBox.Show(msg, "Revamp Bridge", MessageBoxButton.OK, MessageBoxImage.Error);
        Environment.Exit(1);
    }

    static void SetStaticField(Type t, string name, object value)
    {
        FieldInfo f = t.GetField(name, BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public);
        if (f == null) Fail("This .NET Framework has no " + t.FullName + "." + name + " - cannot start the installer.");
        f.SetValue(null, value);
    }

    static string ReadIni(string path, string key)
    {
        if (!File.Exists(path)) return null;
        foreach (string line in File.ReadAllLines(path))
        {
            string l = line.Trim();
            if (l.StartsWith(key + "=", StringComparison.OrdinalIgnoreCase))
                return l.Substring(key.Length + 1).Trim().Trim('"');
        }
        return null;
    }

    static void SetGameDir(string game)
    {
        string dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LB Mod Installer 3");
        string file = Path.Combine(dir, "settings.xml");
        var doc = new XmlDocument();
        if (File.Exists(file)) doc.Load(file);
        XmlElement root = doc.DocumentElement;
        if (root == null)
        {
            doc.AppendChild(doc.CreateXmlDeclaration("1.0", "utf-8", null));
            root = doc.CreateElement("GameDirXml");
            doc.AppendChild(root);
        }
        XmlElement gd = root["GameDirectory"];
        if (gd == null) { gd = doc.CreateElement("GameDirectory"); root.AppendChild(gd); }
        gd.InnerText = game;
        Directory.CreateDirectory(dir);
        doc.Save(file);
    }

    [STAThread]
    static int Main(string[] args)
    {
        // The release exe ("Revamp Bridge.exe") first rebuilds the bridge's
        // files from the player's Revamp zip, then runs the copy of itself
        // shipped under the installer's name (src\Release.cs).
        if (ReleaseSetup.IsRelease && !string.Equals(System.Diagnostics.Process.GetCurrentProcess().ProcessName,
                                                     InstallerName, StringComparison.OrdinalIgnoreCase))
            return ReleaseSetup.Run(args);

        string here = AppDomain.CurrentDomain.BaseDirectory;
        string revamp = null, game = null;
        bool fixOnly = false;
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--revamp" && i + 1 < args.Length) revamp = args[++i];
            else if (args[i] == "--game" && i + 1 < args.Length) game = args[++i];
            else if (args[i] == "--fix-only") fixOnly = true;
        }

        if (fixOnly)
        {
            if (game == null) game = ReadGameDir();
            if (game == null || !File.Exists(Path.Combine(game, "bin", "DBXV2.exe")))
                Fail("Game folder not found - pass --game <folder>.");
            RunPostInstall(Path.GetFullPath(game), here);
            return 0;
        }
        if (revamp == null) revamp = ReadIni(Path.Combine(here, "RevampBridge.ini"), "RevampDir");
        if (string.IsNullOrEmpty(revamp))
            Fail("Revamp folder not set. Put RevampDir=<the Revamp \"Installer\" folder> in RevampBridge.ini, or pass --revamp.");
        revamp = Path.GetFullPath(revamp);

        string exe = Path.Combine(revamp, InstallerName + ".exe");
        if (!File.Exists(exe) || !File.Exists(Path.Combine(revamp, InstallerName + ".installinfo")))
            Fail("\"" + InstallerName + ".exe\" and its .installinfo were not found in:\n" + revamp);

        libDir = Path.Combine(here, "lib");
        if (!File.Exists(Path.Combine(libDir, "xv2corelib.dll")))
            Fail("Missing lib\\xv2corelib.dll - run build.bat first.");

        if (game != null)
        {
            if (!File.Exists(Path.Combine(game, "bin", "DBXV2.exe")))
                Fail("Not a game folder (no bin\\DBXV2.exe):\n" + game);
            SetGameDir(Path.GetFullPath(game));
        }

        // Tracker stamp of the game folder the installer will most likely use
        // (its saved setting), to tell afterwards whether it installed anything.
        string preDir = null, preStamp = null;
        try
        {
            preDir = ReadGameDir();
            if (preDir != null) { preDir = Path.GetFullPath(preDir); preStamp = PostInstall.TrackerStamp(preDir); }
        }
        catch { preDir = null; }

        // Ours must run before Costura's handler, which the installer adds
        // when its module initialises.
        AppDomain.CurrentDomain.AssemblyResolve += Resolve;
        Directory.SetCurrentDirectory(revamp);

        Assembly installer = Assembly.LoadFrom(exe);
        // StartupUri "MainWindow.xaml" is resolved against the resource
        // assembly, which defaults to the entry assembly (this launcher). Its
        // public setter refuses when there is an entry assembly, so set the
        // two fields it would have set.
        SetStaticField(typeof(Application), "_resourceAssembly", installer);
        SetStaticField(typeof(UIElement).Assembly.GetType("System.Windows.Navigation.BaseUriHelper", true),
                       "_resourceAssembly", installer);
        installer.EntryPoint.Invoke(null, null);

        // The installer has closed (Application.Shutdown returns here). Fix up
        // whatever it did in the folder it actually used.
        string used = null;
        try
        {
            used = (string)installer.GetType("LB_Mod_Installer.GeneralInfo", true)
                .GetProperty("GameDir", BindingFlags.Public | BindingFlags.Static).GetValue(null);
        }
        catch { }
        if (used == null || !File.Exists(Path.Combine(used, "bin", "DBXV2.exe"))) return 0;
        used = Path.GetFullPath(used);
        // Closed without installing or uninstalling: leave the game alone.
        // (A folder picked in the installer has no stamp - run the fixes.)
        if (preDir != null && string.Equals(preDir.TrimEnd('\\'), used.TrimEnd('\\'), StringComparison.OrdinalIgnoreCase)
            && PostInstall.TrackerStamp(used) == preStamp)
            return 0;
        RunPostInstall(used, here);
        return 0;
    }

    static void RunPostInstall(string game, string here)
    {
        try
        {
            var log = PostInstall.Run(game, here);
            MessageBox.Show(string.Join("\n", log), "Revamp Bridge - game update fixes", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show("The game update fixes could not be applied:\n\n" + ex.Message +
                            "\n\nOnce the cause is fixed, apply them again without reinstalling: run " +
                            (ReleaseSetup.IsRelease ? "\"Reapply Fixes Only.bat\"." : "this program with --fix-only."),
                            "Revamp Bridge", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    static string ReadGameDir()
    {
        string file = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "LB Mod Installer 3", "settings.xml");
        if (!File.Exists(file)) return null;
        var doc = new XmlDocument();
        doc.Load(file);
        XmlElement gd = doc.DocumentElement == null ? null : doc.DocumentElement["GameDirectory"];
        return gd == null ? null : gd.InnerText;
    }
}
