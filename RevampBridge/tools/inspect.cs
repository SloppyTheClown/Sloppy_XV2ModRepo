// Dump selected vanilla entries from a game folder via Xv2CoreLib (YAX XML).
// inspect <installer exe> <lib dir> <game dir> <game path> <list property path> <index>...
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;

static class P
{
    static string libDir;
    static Dictionary<string, Assembly> cache = new Dictionary<string, Assembly>();
    static Assembly Resolve(object s, ResolveEventArgs e)
    {
        string n = new AssemblyName(e.Name).Name.ToLowerInvariant();
        string p = Path.Combine(libDir, n + ".dll");
        if (!File.Exists(p)) return null;
        lock (cache) { Assembly a; if (!cache.TryGetValue(n, out a)) { a = Assembly.Load(File.ReadAllBytes(p)); cache[n] = a; } return a; }
    }

    static int Main(string[] a)
    {
        libDir = a[1];
        AppDomain.CurrentDomain.AssemblyResolve += Resolve;
        Assembly exe = Assembly.LoadFrom(a[0]);
        System.Runtime.CompilerServices.RuntimeHelpers.RunModuleConstructor(exe.ManifestModule.ModuleHandle);
        MethodInfo parse = exe.GetType("LB_Mod_Installer.Installer.Install", true).GetMethod("GetParsedFileFromGame");
        Assembly core = Assembly.Load("Xv2CoreLib");
        Type ioT = core.GetType("Xv2CoreLib.Resource.Xv2FileIO", true);
        object io = Activator.CreateInstance(ioT, a[2], false, new[] { "data2.cpk", "data1.cpk", "data0.cpk", "data.cpk", "data_d4_5_xv1.cpk" });
        object f = parse.Invoke(null, new object[] { a[3], io, true, true });
        object cur = f;
        foreach (string p in a[4].Split('.')) cur = cur.GetType().GetProperty(p).GetValue(cur);
        Type yaxT = Assembly.Load("YAXLib").GetType("YAXLib.YAXSerializer", true);
        var want = new HashSet<string>(a.Skip(5));
        foreach (object e in (IEnumerable)cur)
        {
            string idx = (string)e.GetType().GetProperty("Index").GetValue(e);
            if (want.Count > 0 && !want.Contains(idx)) continue;
            object ser = Activator.CreateInstance(yaxT, e.GetType());
            Console.WriteLine((string)yaxT.GetMethod("Serialize", new[] { typeof(object) }).Invoke(ser, new[] { e }));
        }
        return 0;
    }
}
