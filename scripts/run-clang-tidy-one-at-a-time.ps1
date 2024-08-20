# Copyright (C) 2024 Fred Emmott <fred@fredemmott.com>
# SPDX-License-Identifier: ISC

param (
    [Parameter(mandatory = $true)]
    [String] $BuildRoot,

    [String[]] $SourceRoot = '.',
  

    [String] $ClangTidy = "clang-tidy"
)

$files = Get-ChildItem -recurse $SourceRoot -include "*.cpp" | resolve-path -relative
while ($files.count -ne 0) {
    $file, $files = $files
    Write-Host "Checking $file..."

    & $ClangTidy -p $BuildRoot $file --quiet
    if ($LASTEXITCODE -ne 0) {
        do {
            Write-Host ('-' * 80)
            Write-Host -NoNewline "(A)bort, (R)etry, or (I)gnore? "
            $key = $Host.UI.RawUI.ReadKey().Character
            Write-Host ""
        } while ($key -notin @('a', 'r', 'i'))
        switch ($Key) {
            'a' <#Abort#> { return; } 
            'r' <#Retry#> {
              $files = @($file) + $files;
              continue;
            }
            'i' <#Ignore#> { continue; }
        }
    }
}
