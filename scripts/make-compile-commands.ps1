# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

param (
  [Parameter(mandatory = $true)]
  [string] $ArgsToLines,
  [Parameter(mandatory = $true)]
  [string] $TrueExe,
  [Parameter(mandatory = $true)]
  [int] $CXXStandard,
  [string] $Compiler,
  [string] $MSBuild = 'msbuild',
  [boolean] $RunMSBuild = $true
)

$extraArgs = @(
  # Make DirectXMath.h includable by Clang
  "/D_XM_NO_INTRINSICS_",
  # Mostly used for TraceLogging fun
  "/DCLANG_TIDY=1"
  # Fixup: As of 2024-08-19, C++23 in MSVC needs /std:c++latest, but we need --std=c++23 for clang-tidy
  "/clang:--std=c++$CXXStandard"
  # Spammy or irrelevant
  "/clang:-Wno-unknown-pragmas"
  "/clang:-Wno-missing-braces"
  "/clang:-Wno-vexing-parse"
)

$removeArgs = @(
  # Other half of the /clang:--std=c++ above
  "/std:c++latest"
  "/std:c++$CXXStandard"
  # This means 'C++'; clang-cl + clang-tidy adds this, which leads to clang-diagnostic-overriding-option
  "/TP"
  # Compile errors can't be suppressed, including some that are unavoidable in WinUI generated code
  "/WX"
  # Leads to uunused-command-line-argument warning, and we want full control here anyway
  "/ZW:nostdlib"
)

if ($RunMSBuild) {
  & $MSBuild com.openkneeboard.sln `
    /t:ClangTidy `
    "/p:ClangTidyToolPath=$((Get-Item $TrueExe).Directory.FullName)" `
    "/p:ClangTidyToolExe=$((Get-Item $TrueExe).Name)"
}

$Entries = @()
$Folders = Get-ChildItem -Path . -Recurse -Filter '*.ClangTidy' | Resolve-Path -Relative

foreach ($Folder in $Folders) {
  $CommandsJSON = Join-Path $Folder "compile_commands.json"
  if (-not (Test-Path $CommandsJSON)) {
    continue;
  }
  $FileCommands = Get-Content -Encoding UTF8 $CommandsJSON | ConvertFrom-Json

  foreach ($FileCommand in $FileCommands) {
    $Source = $FileCommand.file
    # ~ August 2024, MSBuild has started adding a semicolon before additional flags
    $Flags = $FileCommand.command -replace '; ', ''

    # Use cmd.exe so we don't have to deal with replacing \\" with `", along with weird cases like \\\\"
    $_argsToLines, $compiler, $compileArgs = cmd.exe /C "`"${ArgsToLines}`" $Flags"
    $compileArgs = $compileArgs | Where-Object { $_ -notin $removeArgs };
    $compileArgs += $extraArgs;

    if ($Compiler) {
      $compiler = $Compiler;
    }

    # Join-String was introduced in Powershell 6.2, but let's keep working with
    # Windows Powershell (5.1) as cmake calls this
    $compileArgs = ($compileArgs `
    | ForEach-Object { $_ -replace '\\', '\\' } `
    | ForEach-Object { "`"$($_ -replace '"','\"')`"" }
    ) -join ' '

    $Command = "`"$compiler`" $($compileArgs)"
    $Entries += @{
      directory = $FileCommand.directory;
      command   = "$Command";
      file      = "$Source";
    };
  }
}

ConvertTo-Json $Entries | Set-Content -Path "compile_commands.json" -Encoding UTF8
