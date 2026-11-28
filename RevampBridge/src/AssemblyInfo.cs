// File properties shown in Explorer (Details tab). An exe without any version
// info is one of the traits antivirus heuristics weigh against a program, so
// both builds carry it. The version numbers come from VERSION: src\make_release.ps1
// adds them in a generated file; the dev build (build.bat) has none.
using System.Reflection;

[assembly: AssemblyTitle("Revamp Bridge")]
[assembly: AssemblyDescription("Runs the Revamp Xenoverse 2 5.1.1 installer on newer game versions (unofficial, not by the Revamp team)")]
[assembly: AssemblyProduct("Revamp Bridge")]
[assembly: AssemblyCompany("SloppyTheClown")]
[assembly: AssemblyCopyright("SloppyTheClown")]
