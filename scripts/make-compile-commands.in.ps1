# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

$extraArgs = @(
  # Make DirectXMath.h includable by Clang
  "/D_XM_NO_INTRINSICS_",
  # Mostly used for TraceLogging fun
  "/DCLANG_TIDY=1"
  # Fixup: As of 2024-08-19, C++23 in MSVC needs /std:c++latest, but we need --std=c++23 for clang-tidy
  "/clang:--std=c++@CMAKE_CXX_STANDARD@"
  # Spammy or irrelevant
  "/clang:-Wno-unknown-pragmas"
  "/clang:-Wno-missing-braces"
  "/clang:-Wno-vexing-parse"
)

$removeArgs = @(
  # Other half of the /clang:--std=c++ above
  "/std:c++latest"
  "/std:c++@CMAKE_CXX_STANDARD@" 
  # This means 'C++'; clang-cl + clang-tidy adds this, which leads to clang-diagnostic-overriding-option
  "/TP"
)

$Entries = @()
$Folders = Get-ChildItem -Path . -Filter '*.ClCompile_flags.txt' -Recurse | ForEach-Object { Split-Path -parent $_.FullName } | Get-Unique

foreach ($Folder in $Folders) {
  $FlagFiles = Get-ChildItem -Path $Folder -Filter '*.ClCompile_flags.txt' | ForEach-Object { $_.FullName }
  foreach ($FlagFile in $FlagFiles) {
    $SourcesFile = $FlagFile.Replace('ClCompile_flags.txt', 'ClCompile_sources.txt')
    if (!(Test-Path $SourcesFile)) {
      continue;
    }

    # ~ August 2024, MSBuild has started adding a semicolon before additional flags
    $Flags = (Get-Content $FlagFile) `
      -replace ' /D ("?)([^"].+?)\1 ', ' "/D$2" ' `
      -replace ';', ''
    if (!$Flags) {
      continue;
    }

    $compileArgs = Invoke-Expression "echo $($Flags -replace ';','')";
    $compileArgs = $compileArgs | Where-Object { $_ -notin $removeArgs };
    $compileArgs += $extraArgs;

    # Join-String was introduced in Powershell 6.2, but let's keep working with
    # Windows Powershell (5.1) as cmake calls this
    $compileArgs = ($compileArgs | ForEach-Object { "`"$_`"" }) -join ' '

    $Command = "`"@NATIVE_PATH_CMAKE_CXX_COMPILER@`" $($compileArgs)"
    foreach ($Source in $(Get-Content $SourcesFile)) {
      $Entries += @{
        directory = $Folder;
        command   = "$Command `"$Source`"";
        file      = "$Source";
      };
    }
  }
}

ConvertTo-Json $Entries | Set-Content -Path "compile_commands.json" -Encoding UTF8