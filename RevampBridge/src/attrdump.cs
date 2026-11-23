using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;

// attrdump <dll> <refsDir>: prints every custom attribute (with argument
// types and values) on every type, property, field and method, sorted, so two
// builds of the same library can be diffed.
static class P
{
    static int Main(string[] a)
    {
        string refs = a[1];
        AppDomain.CurrentDomain.ReflectionOnlyAssemblyResolve += (s, e) =>
        {
            string n = new AssemblyName(e.Name).Name.ToLowerInvariant();
            string p = Path.Combine(refs, n + ".dll");
            if (File.Exists(p)) return Assembly.ReflectionOnlyLoad(File.ReadAllBytes(p));
            return Assembly.ReflectionOnlyLoad(e.Name);
        };
        Assembly asm = Assembly.ReflectionOnlyLoad(File.ReadAllBytes(a[0]));
        var lines = new List<string>();
        Type[] types;
        try { types = asm.GetTypes(); } catch (ReflectionTypeLoadException ex) { types = ex.Types.Where(t => t != null).ToArray(); }
        const BindingFlags all = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;
        foreach (Type t in types)
        {
            if (t.FullName.Contains("<")) continue;
            Add(lines, t.FullName, t);
            foreach (MemberInfo m in t.GetMembers(all))
                if (!(m.Name.Contains("<")))
                    Add(lines, t.FullName + "::" + m.Name, m);
        }
        lines.Sort(StringComparer.Ordinal);
        foreach (string l in lines.Distinct()) Console.WriteLine(l);
        return 0;
    }

    static void Add(List<string> lines, string owner, MemberInfo m)
    {
        IList<CustomAttributeData> cad;
        try { cad = CustomAttributeData.GetCustomAttributes(m); } catch { return; }
        foreach (var c in cad)
        {
            string name = c.AttributeType.Name;
            if (name.StartsWith("Compiler") || name.StartsWith("DebuggerBrowsable") || name == "NullableAttribute" || name == "NullableContextAttribute"
                || name == "IsReadOnlyAttribute" || name == "AsyncStateMachineAttribute" || name == "IteratorStateMachineAttribute" || name == "DebuggerStepThroughAttribute" || name == "DebuggerHiddenAttribute") continue;
            string args = string.Join(",", c.ConstructorArguments.Select(Fmt))
                + ";" + string.Join(",", c.NamedArguments.Select(n => n.MemberName + "=" + Fmt(n.TypedValue)));
            lines.Add(owner + " [" + name + "(" + args + ")]");
        }
    }

    static string Fmt(CustomAttributeTypedArgument v)
    {
        string val = v.Value == null ? "null" : v.Value is System.Collections.IEnumerable && !(v.Value is string)
            ? "{" + string.Join(",", ((IEnumerable<CustomAttributeTypedArgument>)v.Value).Select(Fmt)) + "}" : v.Value.ToString();
        return v.ArgumentType.Name + ":" + val;
    }
}
