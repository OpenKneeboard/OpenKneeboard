param(
  [switch] $Submit = $false,
  [switch] $Preview = $false,
  [switch] $Stable = $false
)

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

  $extraArgs = @()
  if ($Submit) {
    $extraArgs += "--submit"
  }

  wingetcreate update `
    $PackageID `
    --out $(Get-Item winget).FullName `
    --urls $uri `
    --version $(Get-MSIVersion $path) `
    $extraArgs
}

if ($Preview) {
  Write-Host "FredEmmott.OpenKneeboard.Preview has been removed from winget."
  exit 1
}

if ($Stable) {
  $apiData = Get-Content stable-msi.json | ConvertFrom-Json
  Update-Winget -ApiData $apiData -PackageID FredEmmott.OpenKneeboard
}
