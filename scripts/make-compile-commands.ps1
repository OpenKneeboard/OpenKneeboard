# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

# Make DirectXMath.h includable by Clang
$extraArgs = "-D_XM_NO_INTRINSICS_"

$wasCreated = $false;
$mutex = New-Object System.Threading.Mutex($true, "com.fredemmott.merge-compile-commands", [ref] $wasCreated);
if (!$wasCreated) {
  $mutex.WaitOne() | Out-Null;
}

$Folders = Get-ChildItem -Path . -Filter '*.ClCompile_flags.txt' -Recurse | % { Split-Path -parent $_.FullName } | Get-Unique

foreach ($Folder in $Folders) {
  $FlagFiles = Get-ChildItem -Path $Folder -Filter '*.ClCompile_flags.txt' | % { $_.FullName }
  $Entries = @()
  foreach ($FlagFile in $FlagFiles) {
    $SourcesFile = $FlagFile.Replace('ClCompile_flags.txt', 'ClCompile_sources.txt')
    if (!(Test-Path $SourcesFile)) {
      continue;
    }
    $Command = "`"C:\WINDOWS\system32\CL.exe`" $(Get-Content $FlagFile)"
    foreach ($Source in $(Get-Content $SourcesFile)) {
      $Entries += @{
        directory = $Folder;
        command   = "$command $extraArgs `"$Source`"";
        file      = "$Source";
      };
    }
  }
  ConvertTo-Json $Entries | Set-Content -Path "$Folder/compile_commands.json" -Encoding UTF8
}

$mutex.ReleaseMutex();
