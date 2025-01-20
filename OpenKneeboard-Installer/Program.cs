using System.CommandLine;
using System.Diagnostics;
using System.Runtime.CompilerServices;
using WixSharp;
using WixSharp.CommonTasks;
using File = WixSharp.File;
using System.Text.Json;
using System.Xml.Linq;
using Microsoft.Win32;
using RegistryHive = WixSharp.RegistryHive;

[assembly: InternalsVisibleTo(assemblyName: "OpenKneeboard-Installer.aot")] // assembly name + '.aot suffix

var inputRootArg = new Argument<DirectoryInfo>(
    name: "INPUT_ROOT",
    description: "Location of files to include in the installer");
var signingKeyArg = new Option<string>(
    name: "--signing-key",
    description: "Code signing key ID");
var timestampServerArg = new Option<string>(
    name: "--timestamp-server",
    description: "Code signing timestamp server");
var stampFileArg = new Option<FileInfo>(
    name: "--stamp-file",
    description: "The full path to the produced executable will be written here on success");

var command = new RootCommand("Build the MSI");
command.Add(inputRootArg);
command.Add(signingKeyArg);
command.Add(timestampServerArg);
command.Add(stampFileArg);
command.SetHandler(
    CreateInstaller, inputRootArg, signingKeyArg, timestampServerArg, stampFileArg);
return await command.InvokeAsync(args);

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
    // https://github.com/oleg-shilo/wixsharp/issues/1728
    // project.WxsFiles.Add("installer/WixUI_Minimal_NoEULA.wxs");
    project.UI = WUI.WixUI_Common;
    Compiler.WixSourceGenerated += WixUIWithoutEULA;
}

void WixUIWithoutEULA(XDocument document)
{
    var ui = XElement.Parse(
        """
        <UI Id="file WixUI_Minimal_NoEULA">
            <TextStyle Id="WixUI_Font_Normal" FaceName="Tahoma" Size="8" />
            <TextStyle Id="WixUI_Font_Bigger" FaceName="Tahoma" Size="12" />
            <TextStyle Id="WixUI_Font_Title" FaceName="Tahoma" Size="9" Bold="yes" />
        
            <Property Id="DefaultUIFont" Value="WixUI_Font_Normal" />
        
            <DialogRef Id="ErrorDlg" />
            <DialogRef Id="FatalError" />
            <DialogRef Id="FilesInUse" />
            <DialogRef Id="MsiRMFilesInUse" />
            <DialogRef Id="PrepareDlg" />
            <DialogRef Id="ProgressDlg" />
            <DialogRef Id="ResumeDlg" />
            <DialogRef Id="UserExit" />
            <DialogRef Id="WelcomeDlg" />
        
            <Publish Dialog="ExitDialog" Control="Finish" Event="EndDialog" Value="Return" Order="999" />
        
            <Publish Dialog="VerifyReadyDlg" Control="Back" Event="NewDialog" Value="MaintenanceTypeDlg" />
        
            <Publish Dialog="MaintenanceWelcomeDlg" Control="Next" Event="NewDialog" Value="MaintenanceTypeDlg" />
        
            <Publish Dialog="MaintenanceTypeDlg" Control="RepairButton" Event="NewDialog" Value="VerifyReadyDlg" />
            <Publish Dialog="MaintenanceTypeDlg" Control="RemoveButton" Event="NewDialog" Value="VerifyReadyDlg" />
            <Publish Dialog="MaintenanceTypeDlg" Control="Back" Event="NewDialog" Value="MaintenanceWelcomeDlg" />
        
            <Publish Dialog="WelcomeDlg" Control="Next" Event="NewDialog" Value="VerifyReadyDlg" Condition="(NOT Installed) OR (Installed AND PATCH)" />
            <Publish Dialog="VerifyReadyDlg" Control="Back" Event="NewDialog" Value="WelcomeDlg" Order="2" Condition="(NOT Installed) OR (Installed AND PATCH)" />
        
            <Property Id="ARPNOMODIFY" Value="1" />
        </UI>   
        """);
    var uiRef = XElement.Parse("<UIRef Id=\"WixUI_Common\" />");
    document.Root.Select(Compiler.ProductElementName).Add(ui);
    document.Root.Select(Compiler.ProductElementName).Add(uiRef);
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

    var project =
        new ManagedProject("OpenKneeboard",
            new Dir(@"%ProgramFiles%\OpenKneeboard",
                new Dir("bin",
                    new File(GetExecutableId(), "bin/OpenKneeboardApp.exe"),
                    new Files("bin/*.*", f => !f.EndsWith("OpenKneeboardApp.exe"))),
                new Dir("share", new Files("share/*.*")),
                new Dir("utilities", new Files("utilities/*.*")),
                new Files(installerResources, @"installer/*.*")),
            new Binary(GetMsixRemovalToolId(), "installer/remove-openkneeboard-msix.exe"));
    project.SourceBaseDir = inputRoot.FullName;
    project.ResolveWildCards();

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
    return project;
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