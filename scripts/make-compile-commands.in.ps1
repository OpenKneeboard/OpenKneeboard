# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

$extraArgs = @(
  # Make DirectXMath.h includable by Clang
  "/D_XM_NO_INTRINSICS_",
  # Mostly used for TraceLogging fun
  "/DCLANG_TIDY=1"
)

$Entries = @()
$Folders = Get-ChildItem -Path . -Filter '*.ClCompile_flags.txt' -Recurse | % { Split-Path -parent $_.FullName } | Get-Unique

foreach ($Folder in $Folders) {
  $FlagFiles = Get-ChildItem -Path $Folder -Filter '*.ClCompile_flags.txt' | % { $_.FullName }
  foreach ($FlagFile in $FlagFiles) {
    $SourcesFile = $FlagFile.Replace('ClCompile_flags.txt', 'ClCompile_sources.txt')
    if (!(Test-Path $SourcesFile)) {
      continue;
    }
    $Command = "`"@NATIVE_PATH_CMAKE_CXX_COMPILER@`" $($extraArgs -join ' ') $(Get-Content $FlagFile)"
    foreach ($Source in $(Get-Content $SourcesFile)) {
      $Entries += @{
        directory = $Folder;
        command   = "$command `"$Source`"";
        file      = "$Source";
      };
    }
  }
}

ConvertTo-Json $Entries | Set-Content -Path "compile_commands.json" -Encoding UTF8