// ID collision check between a mod install and a game update.
//
// For every file/section in an LB Mod Installer tracker XML, loads the vanilla
// file from two game folders (old = the version the mod was made for, new =
// the current one), collects the Index of every installable entry per list,
// and reports IDs the mod installed that are vanilla in NEW but not in OLD:
// those overwrote something the update added.
//
// idcollide <installer exe> <lib dir> <tracker.xml> <old game dir> <new game dir>
using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Xml;

static class P
{
    static string libDir;
    static Dictionary<string, Assembly> cache = new Dictionary<string, Assembly>();
    static Assembly Resolve(object s, ResolveEventArgs e)
    {
        string n = new AssemblyName(e.Name).Name.ToLowerInvariant();
        string p = Path.Combine(libDir, n + ".dll");
        if (!File.Exists(p)) return null;
        lock (cache)
        {
            Assembly a;
            if (!cache.TryGetValue(n, out a)) { a = Assembly.Load(File.ReadAllBytes(p)); cache[n] = a; }
            return a;
        }
    }

    static MethodInfo parse;
    static Type installable;

    // list property name -> set of Index values, recursively (depth-limited)
    static void Collect(object o, string path, Dictionary<string, HashSet<string>> into, int depth)
    {
        if (o == null || depth > 3) return;
        foreach (PropertyInfo p in o.GetType().GetProperties(BindingFlags.Public | BindingFlags.Instance))
        {
            if (p.GetIndexParameters().Length > 0) continue;
            Type t = p.PropertyType;
            if (t == typeof(string) || t.IsPrimitive || t.IsEnum) continue;
            object v;
            try { v = p.GetValue(o); } catch { continue; }
            if (v == null) continue;
            IEnumerable list = v as IEnumerable;
            if (list != null && !(v is string))
            {
                Type et = t.IsGenericType ? t.GetGenericArguments()[0] : null;
                if (et != null && installable.IsAssignableFrom(et))
                {
                    var set = new HashSet<string>();
                    PropertyInfo idx = installable.GetProperty("Index");
                    foreach (object e in list)
                    {
                        string id = null;
                        try { id = (string)idx.GetValue(e); set.Add(id); } catch { }
                        // children are recorded both merged and per parent (name@parent)
                        var sub = new Dictionary<string, HashSet<string>>();
                        Collect(e, "", sub, depth + 1);
                        foreach (var kv in sub)
                        {
                            string merged = path + p.Name + "." + kv.Key;
                            if (!into.ContainsKey(merged)) into[merged] = new HashSet<string>();
                            into[merged].UnionWith(kv.Value);
                            if (id != null) into[merged + "@" + id] = kv.Value;
                        }
                    }
                    into[path + p.Name] = set;
                }
                else if (et != null && et.Namespace != null && et.Namespace.StartsWith("Xv2CoreLib"))
                {
                    // plain container list (e.g. PSC Configurations): look inside, merged
                    foreach (object e in list)
                    {
                        var sub = new Dictionary<string, HashSet<string>>();
                        Collect(e, "", sub, depth + 1);
                        foreach (var kv in sub)
                        {
                            if (!into.ContainsKey(kv.Key)) into[kv.Key] = new HashSet<string>();
                            into[kv.Key].UnionWith(kv.Value);
                        }
                    }
                }
            }
            else if (t.IsClass && t.Namespace != null && t.Namespace.StartsWith("Xv2CoreLib"))
                Collect(v, path + p.Name + ".", into, depth + 1);
        }
    }

    static Dictionary<string, HashSet<string>> Lists(object io, string path)
    {
        var d = new Dictionary<string, HashSet<string>>();
        object f;
        try { f = parse.Invoke(null, new object[] { path, io, true, false }); }
        catch (Exception ex) { while (ex.InnerException != null) ex = ex.InnerException; d["!error"] = new HashSet<string> { ex.Message }; return d; }
        if (f == null) return null;
        Collect(f, "", d, 0);
        return d;
    }

    static string Norm(string s) { return s.ToLowerInvariant().Replace("_", "").Replace("s", "").Replace("entrie", "entry"); }

    static int Main(string[] a)
    {
        libDir = a[1];
        AppDomain.CurrentDomain.AssemblyResolve += Resolve;
        Assembly exe = Assembly.LoadFrom(a[0]);
        System.Runtime.CompilerServices.RuntimeHelpers.RunModuleConstructor(exe.ManifestModule.ModuleHandle);
        parse = exe.GetType("LB_Mod_Installer.Installer.Install", true).GetMethod("GetParsedFileFromGame");
        Assembly core = Assembly.Load("Xv2CoreLib");
        installable = core.GetType("Xv2CoreLib.IInstallable", true);
        Type ioT = core.GetType("Xv2CoreLib.Resource.Xv2FileIO", true);
        string[] cpks = { "data2.cpk", "data1.cpk", "data0.cpk", "data.cpk", "data_d4_5_xv1.cpk" };
        object oldIo = Activator.CreateInstance(ioT, a[3], false, cpks);
        object newIo = Activator.CreateInstance(ioT, a[4], false, cpks);

        var doc = new XmlDocument(); doc.Load(a[2]);
        int checkedSections = 0, collisions = 0;
        foreach (XmlElement file in doc.SelectNodes("//File"))
        {
            string path = file.GetAttribute("FilePath").Replace('\\', '/');
            var secs = file.SelectNodes("Section").Cast<XmlElement>()
                .Where(s => s.GetAttribute("IDs").Length > 0).ToList();
            if (secs.Count == 0) continue;
            string ext = Path.GetExtension(path).ToLowerInvariant();
            if (ext == ".msg" || ext == ".acb" || ext == ".emz" || ext == ".emb" || ext == ".xml" || ext == ".x2s") continue;
            var oldL = Lists(oldIo, path);
            var newL = Lists(newIo, path);
            if (newL == null || oldL == null) continue;  // file new in one version: nothing to collide with
            if (newL.ContainsKey("!error") || oldL.ContainsKey("!error"))
            {
                Console.WriteLine("?? " + path + ": " + string.Join(" | ", (newL.ContainsKey("!error") ? newL : oldL)["!error"]));
                continue;
            }
            foreach (XmlElement s in secs)
            {
                string sec = s.GetAttribute("FileSection");
                string key = sec.Contains("_") ? sec.Substring(sec.IndexOf('_') + 1) : sec;
                // parent-scoped sections: "X/7", "X/1_-1" (parent 1), "PSC_Entry_40"
                string parent = null;
                if (key.Contains('/')) { parent = key.Split('/')[1].Split('_')[0]; key = key.Split('/')[0]; }
                else
                {
                    var m = System.Text.RegularExpressions.Regex.Match(key, @"^(.*?)_(\d+)$");
                    if (m.Success && sec.StartsWith("PSC")) { parent = m.Groups[2].Value; key = m.Groups[1].Value; }
                }
                Func<string, string> last = k => k.Split('@')[0].Split('.').Last();
                var pool = parent != null ? newL.Keys.Where(k => k.EndsWith("@" + parent)).ToList() : newL.Keys.Where(k => !k.Contains("@")).ToList();
                if (pool.Count == 0 && parent != null) pool = newL.Keys.Where(k => !k.Contains("@") && k.Contains(".")).ToList();
                // pick the list whose (last) property name matches the section
                string best = pool.FirstOrDefault(k => Norm(last(k)) == Norm(key))
                           ?? pool.FirstOrDefault(k => Norm(last(k)).Contains(Norm(key)) || Norm(key).Contains(Norm(last(k))))
                           ?? (pool.Count == 1 ? pool[0] : null);
                if (best == null)
                {
                    Console.WriteLine("-- " + path + " [" + sec + "]: no matching list (" + string.Join(",", newL.Keys) + ")");
                    continue;
                }
                checkedSections++;
                HashSet<string> o = oldL.ContainsKey(best) ? oldL[best] : new HashSet<string>();
                HashSet<string> n = newL[best];
                var ids = s.GetAttribute("IDs").Split(',');
                var hit = ids.Where(id => n.Contains(id) && !o.Contains(id)).ToList();
                if (hit.Count > 0)
                {
                    collisions += hit.Count;
                    Console.WriteLine("!! " + path + " [" + sec + " -> " + best + "]: " + string.Join(",", hit.Take(30)) + (hit.Count > 30 ? " ..." : ""));
                }
            }
        }
        Console.WriteLine("sections checked: " + checkedSections + ", colliding IDs: " + collisions);
        return 0;
    }
}

