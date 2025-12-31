using System.CommandLine;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using WixSharp;
using WixSharp.CommonTasks;
using File = WixSharp.File;
using System.Text.Json;
using System.Xml;
using System.Xml.Linq;
using System.Xml.XPath;
using Microsoft.Win32;
using WixToolset.Dtf.WindowsInstaller;
using RegistryHive = WixSharp.RegistryHive;

[assembly: InternalsVisibleTo(assemblyName: "OpenKneeboard-Installer.aot")] // assembly name + '.aot suffix

var inputRootArg = new Argument<DirectoryInfo>("INPUT_ROOT")
{
    Description = "Location of files to include in the installer",
};
var signingKeyArg = new Option<string>("--signing-key")
{
    Description = "Code signing key ID",
};
var timestampServerArg = new Option<string>("--timestamp-server")
{
    Description = "Code signing timestamp server",
};
var stampFileArg = new Option<FileInfo>("--stamp-file")
{
    Description = "The full path to the produced executable will be written here on success",
};

var command = new RootCommand("Build the MSI");
command.Add(inputRootArg);
command.Add(signingKeyArg);
command.Add(timestampServerArg);
command.Add(stampFileArg);
command.SetAction(args =>
    CreateInstaller(args.GetRequiredValue(inputRootArg), args.GetValue(signingKeyArg),
        args.GetValue(timestampServerArg), args.GetValue(stampFileArg)));
return await command.Parse(args).InvokeAsync();

async Task SetProjectVersionFromJson(ManagedProject project, DirectoryInfo inputRoot)
{
    var stream = System.IO.File.OpenRead(inputRoot.GetFiles("installer/version.json").First().FullName);
    var options = new JsonSerializerOptions
    {
        PropertyNameCaseInsensitive = true,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
    };

    var version = await JsonSerializer.DeserializeAsync<JsonVersionInfo>(stream, options);
    Debug.Assert(version != null, nameof(version) + " != null");
    var c = version.Components;
    project.Version = new Version($"{c.A}.{c.B}.{c.C}.{c.D}");
    project.OutFileName += $"-v{c.A}.{c.B}.{c.C}";
    if (!version.Tagged)
    {
        project.OutFileName += $"+{version.TweakLabel}.{c.D}";
    }

    project.AddRegKey(
        new RegKey(
            project.DefaultFeature,
            RegistryHive.LocalMachine,
            @"SOFTWARE\Fred Emmott\OpenKneeboard\Version",
            new RegValue("Semantic", version.Readable),
            new RegValue("Readable", $"v{version.Readable}"),
            new RegValue("Major", version.Components.A),
            new RegValue("Minor", version.Components.B),
            new RegValue("Build", version.Components.C),
            new RegValue("Tweak", version.Components.D),
            new RegValue("Triple", $"{version.Components.A}.{version.Components.B}.{version.Components.C}"),
            new RegValue("Quad",
                $"{version.Components.A}.{version.Components.B}.{version.Components.C}.{version.Components.D}")
        ));
}

HardLink[] GetHardLinks(DirectoryInfo inputRoot)
{
    var links = new List<HardLink>();
    foreach (var file in inputRoot.GetFiles("*-aliases.txt", new EnumerationOptions { RecurseSubdirectories = true }))
    {
        var directory = Path.GetRelativePath(inputRoot.FullName, file.Directory!.FullName);
        var target = Path.Combine(directory, file.Name.Replace("-aliases.txt", ".exe"));
        var aliases = System.IO.File.ReadAllLines(file.FullName, Encoding.UTF8)
            .Where(l => !string.IsNullOrWhiteSpace(l))
            .Select(l => Path.Combine(directory, l) + ".exe");
        links.AddRange(aliases.Select(l => new HardLink(target, l)));
    }

    return links.ToArray();
}

async Task<int> CreateInstaller(DirectoryInfo inputRoot, string? signingKeyId, string? timestampServer,
    FileInfo? stampFile)
{
    if (!System.IO.File.Exists($"{inputRoot}/bin/OpenKneeboardApp.exe"))
    {
        Console.WriteLine($"Cannot find bin/OpenKneeboardApp.exe in INPUT_ROOT ({inputRoot.FullName})");
        return 1;
    }

    var project = CreateProject(inputRoot);
    project.EnableResilientPackage();
    RemoveLicenseDialog(project);
    await SetProjectVersionFromJson(project, inputRoot);

    CreateShortcuts(inputRoot, project);
    AddFileTypeRegistration(project);

    RegisterApiLayers(inputRoot, project);
    project.AfterInstall += DisableLegacyApiLayers;
    project.AddAction(
        new BinaryFileAction(GetMsixRemovalToolId().Value, "Removing legacy MSIX versions of OpenKneeboard...",
            Return.check, When.After, Step.InstallFiles, Condition.Always)
        {
            Impersonate = false,
            Execute = Execute.deferred,
        });

    SignProject(project, signingKeyId, timestampServer);
    BuildMsi(project, stampFile);

    return 0;
}

Id GetMsixRemovalToolId()
{
    return new Id("Binary_remove_openkneeboard_msix_exe");
}

Id GetExecutableId()
{
    return new Id("File_Bin_OpenKneeboardApp_exe");
}

void RemoveLicenseDialog(ManagedProject project)
{
    project.UI = WUI.WixUI_Common;
    project.WxsFiles.Add("installer/WixUI_Minimal_NoEULA.wxs");
    Compiler.WixSourceGenerated += document =>
    {
        document.Root.Select(Compiler.ProductElementName).Add(
            XElement.Parse("<UIRef Id=\"WixUI_Minimal_NoEULA\" />")
        );
    };
}

void SignProject(ManagedProject managedProject, string? s, string? timestampServer1)
{
    if (s != null)
    {
        managedProject.DigitalSignature = new DigitalSignature
        {
            CertificateId = s,
            CertificateStore = StoreType.sha1Hash,
            HashAlgorithm = HashAlgorithmType.sha256,
            Description = managedProject.OutFileName,
            TimeUrl = (timestampServer1 == null) ? null : new Uri(timestampServer1),
        };
        managedProject.SignAllFiles = true;
    }
    else
    {
        managedProject.OutFileName += "-UNSIGNED";
    }
}

void BuildMsi(ManagedProject managedProject, FileInfo? fileInfo)
{
    var outFile = managedProject.BuildMsi();
    if (fileInfo != null)
    {
        System.IO.File.WriteAllText(fileInfo.FullName, $"{outFile}\n");
        Console.WriteLine($"Wrote output path '{outFile}' to `{fileInfo.FullName}`");
    }
}


ManagedProject CreateProject(DirectoryInfo inputRoot)
{
    var installerResources = new Feature("Installer Resources");
    installerResources.IsEnabled = false;

    var hardLinks = GetHardLinks(inputRoot);

    var project =
        new ManagedProject("OpenKneeboard",
            new Dir(@"%ProgramFiles%\OpenKneeboard",
                new Dir("bin",
                    new File(GetExecutableId(), "bin/OpenKneeboardApp.exe"),
                    new Files("bin/*.*", f => !f.EndsWith("OpenKneeboardApp.exe"))),
                new Dir("share", new Files("share/*.*")),
                new Dir("libexec", new Files("libexec/*.*")),
                new Dir("utilities",
                    new Files("utilities/*.exe",
                        f => hardLinks.All(l => Path.Combine(inputRoot.FullName, l.Link) != f))),
                new Dir("include", new Files("include/*.*")),
                new Dir("scripts", new Files("scripts/*.*")),
                new Files(installerResources, @"installer/*.*")),
            new Binary(GetMsixRemovalToolId(), "installer/remove-openkneeboard-msix.exe"));
    project.AddProperty(new Property("OKB_HARDLINKS", hardLinks.Select(l => l.ToString()).JoinBy(",")));
    project.SourceBaseDir = inputRoot.FullName;
    project.ResolveWildCards();

    project.EnableUpgradingFromPerUserToPerMachine();
    Compiler.PreserveTempFiles = true;

    project.UpgradeCode = Guid.Parse("843c9331-0610-4ab1-9cf9-5305c896fb5b");
    project.ProductId = Guid.NewGuid();
    project.Platform = Platform.x64;

    project.ControlPanelInfo.Manufacturer = "Fred Emmott";
    project.LicenceFile = "installer/LICENSE.rtf";

    project.BannerImage = "installer/assets/WixUIBanner.png";
    project.BackgroundImage = "installer/assets/WixUIDialog.png";
    project.ValidateBackgroundImage = false;
    project.ControlPanelInfo.ProductIcon = inputRoot.FullName + "/installer/assets/icon.ico";

    project.ControlPanelInfo.InstallLocation = "[INSTALLDIR]";
    project.AddRegValue(new RegValue(RegistryHive.LocalMachine, @"SOFTWARE\Fred Emmott\OpenKneeboard", "InstallDir",
        "[INSTALLDIR]"));
    project.AddRegValue(new RegValue(RegistryHive.LocalMachine, @"SOFTWARE\Fred Emmott\OpenKneeboard\Installer",
        "ProductGuid", $"{{{project.ProductId?.ToString().ToUpper()}}}"));
    project.AddRegValue(new RegValue(RegistryHive.LocalMachine, @"SOFTWARE\Fred Emmott\OpenKneeboard\Installer",
        "UpgradeGuid", $"{{{project.UpgradeCode?.ToString().ToUpper()}}}"));

    project.AddActions(
        new ManagedAction(CustomActions.InstallHardLinks, CustomActions.UninstallHardLinks)
        {
            Condition = Condition.NOT_BeingRemoved,
            UsesProperties = "OKB_HARDLINKS",
            When = When.After,
            Step = Step.InstallFiles,
            Execute = Execute.deferred,
            Impersonate = false,
        },
        new ManagedAction(CustomActions.UninstallHardLinks)
        {
            Condition = Condition.BeingUninstalled,
            UsesProperties = "OKB_HARDLINKS",
            When = When.Before,
            Step = Step.RemoveFiles,
            Execute = Execute.deferred,
            Impersonate = false,
        }
    );

    return project;
}

void AddFileTypeRegistration(ManagedProject project)
{
    project.WxsFiles.Add("installer/OpenKneeboardPluginFileType.wxs");
    Compiler.WixSourceGenerated += document =>
    {
        var xmlns = new XmlNamespaceManager(new NameTable());
        xmlns.AddNamespace("wix", "http://wixtoolset.org/schemas/v4/wxs");
        document.Root.XPathSelectElement("//wix:Feature[@Id='Complete']", xmlns).Add(
            XElement.Parse("<ComponentRef Id=\"OpenKneeboardPlugin_FileType\" />")
        );
    };
}

void RegisterApiLayers(DirectoryInfo directoryInfo, ManagedProject managedProject)
{
    const string apiLayersKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit";
    managedProject.AddRegValue(new RegValue(RegistryHive.LocalMachine, apiLayersKey,
        $"[INSTALLDIR]bin\\OpenKneeboard-OpenXR.json", 0));
    var value = new RegValue(RegistryHive.LocalMachine, apiLayersKey,
        $"[INSTALLDIR]bin\\OpenKneeboard-OpenXR32.json", 0);
    value.Win64 = false;
    managedProject.AddRegValue(value);
}

void DisableLegacyApiLayers(SetupEventArgs args)
{
    if (!args.IsElevated)
    {
        return;
    }

    if (!args.IsInstalling)
    {
        return;
    }

    var installedPath = Path.GetFullPath(args.InstallDir + @"\bin\OpenKneeboard-OpenXR.json");

    DisableLegacyApiLayersInRoot(Registry.LocalMachine, pathToKeep: installedPath);
    foreach (var name in Registry.Users.GetSubKeyNames())
    {
        var user = Registry.Users.OpenSubKey(name, writable: true);
        if (user == null)
        {
            continue;
        }

        DisableLegacyApiLayersInRoot(user, pathToKeep: installedPath);
    }
}

void DisableLegacyApiLayersInRoot(RegistryKey root, string pathToKeep)
{
    const string apiLayersKey = @"SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit";
    var layers = root.OpenSubKey(apiLayersKey, writable: true);
    if (layers == null)
    {
        return;
    }

    foreach (var jsonPath in layers.GetValueNames())
    {
        if (!jsonPath.Contains("OpenKneeboard"))
        {
            continue;
        }

        var regNormalized = Path.GetFullPath(jsonPath);
        if (regNormalized != pathToKeep)
        {
            layers.SetValue(jsonPath, 1, RegistryValueKind.DWord);
        }
    }
}

void CreateShortcuts(DirectoryInfo directoryInfo, ManagedProject managedProject)
{
    var file = managedProject.FindFile((file) => file.Id == GetExecutableId()).First();
    file.AddShortcuts(
        new FileShortcut("OpenKneeboard", "INSTALLDIR"),
        new FileShortcut("OpenKneeboard", "%Desktop%"),
        new FileShortcut("OpenKneeboard", "%ProgramMenuFolder%"));
}

class JsonVersionComponents
{
    public int A { get; set; }
    public int B { get; set; }
    public int C { get; set; }
    public int D { get; set; }
}

class JsonVersionInfo
{
    public JsonVersionComponents Components { get; set; } = new JsonVersionComponents();
    public string Readable { get; set; } = string.Empty;
    public string TweakLabel { get; set; } = string.Empty;
    public bool Stable { get; set; }
    public bool Tagged { get; set; }
}

record HardLink(string Target, string Link) : IParsable<HardLink>
{
    public override string ToString() => $"{Target}|{Link}";

    public static HardLink Parse(string s, IFormatProvider? provider)
    {
        if (TryParse(s, provider, out var result))
        {
            return result;
        }

        throw new FormatException($"Unable to parse '{s}' as HardLink. Expected format: 'Target|Link'");
    }

    public static bool TryParse(string? s, IFormatProvider? provider, out HardLink result)
    {
        result = null!;

        if (string.IsNullOrWhiteSpace(s))
        {
            return false;
        }

        var parts = s.Split('|', 2);
        if (parts.Length != 2)
        {
            return false;
        }

        result = new HardLink(parts[0].Trim(), parts[1].Trim());
        return true;
    }

    public void AddReference(DirectoryInfo root, RegistryKey key)
    {
        var link = Path.Combine(root.FullName, Link);
        var target = Path.Combine(root.FullName, Target);

        if (System.IO.File.Exists(link))
        {
            System.IO.File.Delete(link);
        }

        CreateHardLinkW(link, target, IntPtr.Zero);

        var value = (int)key.GetValue(Link, 0);
        value += 1;
        key.SetValue(Link, value);
    }

    public void RemoveReference(DirectoryInfo root, RegistryKey key)
    {
        var value = (int)key.GetValue(Link, 0);
        value -= 1;
        key.SetValue(Link, value);

        var link = Path.Combine(root.FullName, Link);
        if (value == 0 && System.IO.File.Exists(link))
        {
            System.IO.File.Delete(link);
        }
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    [return: MarshalAs(UnmanagedType.Bool)]
    static extern bool CreateHardLinkW(
        string lpFileName,
        string lpExistingFileName,
        IntPtr lpSecurityAttributes
    );
}

public class CustomActions
{
    [CustomAction]
    public static ActionResult InstallHardLinks(Session session)
    {
        try
        {
            return HandleHardLinks(session, isUninstalling: false);
        }
        catch (Exception e)
        {
            session.Log($"Error installing hard links: {e}");
            return ActionResult.Failure;
        }
    }

    [CustomAction]
    public static ActionResult UninstallHardLinks(Session session)
    {
        try
        {
            return HandleHardLinks(session, isUninstalling: true);
        }
        catch (Exception e)
        {
            session.Log($"Error uninstalling hard links: {e}");
            return ActionResult.Failure;
        }
    }

    private static ActionResult HandleHardLinks(Session session, bool isUninstalling)
    {
        const string RegKeyPath = @"SOFTWARE\Fred Emmott\OpenKneeboard\Installer\HardLinks";
        string rawLinks = session.Property("OKB_HARDLINKS");
        string installDir = session.Property("INSTALLDIR");

        if (string.IsNullOrEmpty(rawLinks))
            return ActionResult.Failure;

        var links = rawLinks.Split(",").Select(s => HardLink.Parse(s, null));
        var root = new DirectoryInfo(installDir);
        using var regKey = Registry.LocalMachine.CreateSubKey(RegKeyPath);

        foreach (var link in links)
        {
            if (isUninstalling)
                link.RemoveReference(root, regKey);
            else
                link.AddReference(root, regKey);
        }

        if (!isUninstalling)
            return ActionResult.Success;

        foreach (var name in regKey.GetValueNames())
            if ((int)regKey.GetValue(name, 0) <= 0)
                regKey.DeleteValue(name);

        if (regKey.ValueCount > 0)
            return ActionResult.Success;

        regKey.Close();
        Registry.LocalMachine.DeleteSubKey(RegKeyPath);

        return ActionResult.Success;
    }
}