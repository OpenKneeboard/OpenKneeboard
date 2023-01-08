# This script requires PowerShell 6 or above

# All releases, in version order
$AllReleases = (Invoke-WebRequest -URI https://api.github.com/repos/OpenKneeboard/OpenKneeboard/releases).Content
| ConvertFrom-Json
| Sort-Object -Descending -Property { [System.Management.Automation.SemanticVersion] ($_.tag_name -replace '^v') }

# Latest release, regardless of if it's a prerelease or not
$LatestAny = $AllReleases | Select-Object -First 1
$LatestAny
| ConvertTo-Json -Depth 8 -AsArray
| Out-File -Encoding utf8NoBOM -FilePath preview-msi.json
# As above, but only MSIX releases, for compatibility with older autoupdaters
$AllReleases
| Where-Object { $_.assets.Where( { $_.name -Like "*.msix" } ).Count -gt 0 }
| Select-Object -First 1
| ConvertTo-Json -Depth 8 -AsArray
| Out-File -Encoding utf8NoBOM -FilePath preview.json

# Latest stable release
$AllReleases
| Where-Object -Not -Property 'prerelease'
| Select-Object -First 1
| ConvertTo-Json -Depth 8 -AsArray
| Out-File -Encoding utf8NoBOM -FilePath stable-msi.json
# As above, but only MSIX releases, for compatibility with older autoupdaters
$AllReleases
| Where-Object { $_.assets.Where( { $_.name -Like "*.msix" } ).Count -gt 0 }
| Where-Object -Not -Property 'prerelease'
| Select-Object -First 1
| ConvertTo-Json -Depth 8 -AsArray
| Out-File -Encoding utf8NoBOM -FilePath stable.json

function Get-MSIVersion {
  param (
    [string] $msiPath
  )

  $msiPath = (Resolve-Path $msiPath).Path

  $windowsInstaller = New-Object -com WindowsInstaller.Installer

  $database = $windowsInstaller.GetType().InvokeMember(
    "OpenDatabase", "InvokeMethod", $Null, 
    $windowsInstaller, @($msiPath, 0)
  )

  $query = "SELECT Value FROM Property WHERE Property = 'ProductVersion'"
  $view = $database.GetType().InvokeMember(
    "OpenView", "InvokeMethod", $Null, $database, ($query)
  )

  $view.GetType().InvokeMember("Execute", "InvokeMethod", $Null, $view, $Null)

  $record = $view.GetType().InvokeMember(
    "Fetch", "InvokeMethod", $Null, $view, $Null
  )

  $version = $record.GetType().InvokeMember(
    "StringData", "GetProperty", $Null, $record, 1
  )

  $view.GetType().InvokeMember("Close", "InvokeMethod", $Null, $view, $Null)

  return $version
}

function Update-Winget {
  param (
    [Object] $ApiData,
    [string] $PackageID
  )

  $uri = "$($ApiData.assets.Where( { $_.name -Like "*.msi" } ).browser_download_url | Select-Object -First 1)"
  $path = New-TemporaryFile
  Invoke-WebRequest -Uri $uri -OutFile $path

  wingetcreate update `
    $PackageID `
    --out $(Get-Item winget).FullName `
    --urls $uri `
    --version $(Get-MSIVersion $path) `
    --submit
}
Update-Winget -ApiData $LatestAny -PackageID FredEmmott.OpenKneeboard.Preview
