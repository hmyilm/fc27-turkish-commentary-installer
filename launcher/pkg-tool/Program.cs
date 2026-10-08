// SPDX-License-Identifier: GPL-3.0-or-later
// Packages the freestanding launcher bridge, not a payload ELF as eboot.bin.
using System.Security.Cryptography;
using System.Text.Json;
using LibProsperoPkg;
using LibProsperoPkg.Content;
using LibProsperoPkg.PKG;

if (args.Length != 2)
{
    Console.Error.WriteLine("Usage: PackageLauncher <prepared-app-folder> <output-folder>");
    return 2;
}

string source = Path.GetFullPath(args[0]);
string output = Path.GetFullPath(args[1]);
const string contentId = "UP9000-PPSA99027_00-FCTRINSTALLER001";
const string titleId = "PPSA99027";
const string packageTitle = "FC27 Turkce Spiker Kurucu";
if (Directory.Exists(output) && Directory.EnumerateFileSystemEntries(output).Any())
    throw new InvalidOperationException("Output folder is not empty; use a fresh output folder.");
Directory.CreateDirectory(output);
byte[] originalBridge = File.ReadAllBytes(Path.Combine(source, "eboot.bin"));
var signingOptions = new FselfOptions
{
    AppVersion = 0x0000000001000000UL,
    FirmwareVersion = 0,
};

var result = ProsperoHomebrewPackager.Package(new ProsperoHomebrewPackageOptions
{
    HomebrewFolder = source,
    OutputFolder = output,
    ContentId = contentId,
    Title = packageTitle,
    Version = "01.00",
    KeepStaging = true,
    FselfOptions = signingOptions,
}, Console.WriteLine);

// Read the package back and independently extract its application filesystem.
// A structural round trip does not establish console acceptance or app startup.
string extracted = Path.Combine(output, "roundtrip-app");
if (Directory.Exists(extracted))
    throw new InvalidOperationException("Round-trip output exists; use a fresh output folder.");
var manifest = ProsperoPackageExtractor.Extract(result.OutputPath, extracted,
    ProsperoExtractionKey.FromPasscode(new string('0', 32)),
    new ProsperoExtractionOptions { ExtractOuterMetadata = true, IncludeNestedImageRaw = true },
    Console.WriteLine);
string extractedModule = Path.Combine(extracted, "eboot.bin");
if (!File.Exists(extractedModule))
    throw new InvalidOperationException("Round-trip extraction did not recover eboot.bin.");
byte[] recovered = File.ReadAllBytes(extractedModule);
if (!ProsperoFself.IsSelf(recovered))
    throw new InvalidOperationException("Package launcher is not a native PS5 SELF.");
byte[] expectedSelf = ProsperoFself.MakeFself(originalBridge, signingOptions);
if (!recovered.AsSpan().SequenceEqual(expectedSelf))
    throw new InvalidOperationException("Recovered PS5 SELF differs from the signed source bridge.");
byte[] originalAuxiliary = File.ReadAllBytes(Path.Combine(source, "sce_sys/about/right.sprx"));
byte[] recoveredAuxiliary = File.ReadAllBytes(Path.Combine(extracted, "sce_sys/about/right.sprx"));
byte[] expectedAuxiliary = ProsperoFself.MakeFself(originalAuxiliary, signingOptions);
if (!recoveredAuxiliary.AsSpan().SequenceEqual(expectedAuxiliary))
    throw new InvalidOperationException("Recovered auxiliary SELF differs from our own source module.");
var expectedInnerPaths = new[] { "eboot.bin", "sce_sys/about/right.sprx", "sce_sys/keystone" };
var actualInnerPaths = manifest.Entries.Select(e => e.RelativePath.Replace('\\', '/')).Order().ToArray();
if (!actualInnerPaths.SequenceEqual(expectedInnerPaths.Order()))
    throw new InvalidOperationException("Unexpected application files in the package.");
var self = ProsperoFself.Parse(recovered);
if (self.ExtInfo?.AuthorityId != ProsperoFself.FakeAuthorityId)
    throw new InvalidOperationException("Unexpected fake SELF authority.");
using var fs = File.OpenRead(result.OutputPath);
var package = ProsperoPkgReader.Read(fs);
foreach (string name in new[] { "param.json", "icon0.png" })
{
    var entry = package.Entries.Single(e => e.Name == name);
    if (entry.Encrypted)
        throw new InvalidOperationException("Expected plaintext app metadata: " + name);
    fs.Position = (long)package.Fih!.EmbeddedCntOffset + entry.DataOffset;
    byte[] recoveredMetadata = new byte[entry.DataSize];
    fs.ReadExactly(recoveredMetadata);
    byte[] expectedMetadata = File.ReadAllBytes(Path.Combine(result.StagingFolder, "sce_sys", name));
    if (!recoveredMetadata.AsSpan().SequenceEqual(expectedMetadata))
        throw new InvalidOperationException("Recovered metadata differs: " + name);
}
fs.Position = 0;
var info = new
{
    content_id = contentId,
    title_id = titleId,
    package_file = Path.GetFileName(result.OutputPath),
    package_bytes = fs.Length,
    sha256 = Convert.ToHexStringLower(SHA256.HashData(fs)),
    native_ps5_self_magic = $"{ProsperoFself.Magic:x8}",
    bridge_elf_sha256 = Convert.ToHexStringLower(SHA256.HashData(originalBridge)),
    bridge_self_sha256 = Convert.ToHexStringLower(SHA256.HashData(recovered)),
    auxiliary_self_sha256 = Convert.ToHexStringLower(SHA256.HashData(recoveredAuxiliary)),
    inner_files = actualInnerPaths,
    metadata_roundtrip = "param.json and icon0.png match staged application",
    library_portability = "portable SHA3 fallback; single-padding inner-reader correction",
    library_commit = "748eabf1b7d17819528cabf367d8e27109d8fce3",
    structural_launch_readiness = result.LaunchReadiness.IsLaunchReady,
    console_acceptance = "not tested",
    console_launch = "not tested",
    warnings = result.Warnings,
};
File.WriteAllText(Path.Combine(output, "package-build-result.json"),
    JsonSerializer.Serialize(info, new JsonSerializerOptions { WriteIndented = true }));
Console.WriteLine(JsonSerializer.Serialize(info, new JsonSerializerOptions { WriteIndented = true }));
return 0;
