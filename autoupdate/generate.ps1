# This script requires PowerShell 6 or above
param(
  [switch] $Preview = $false,
  [switch] $Stable = $false,
  [Parameter()]
  [string] $GithubToken
)

$Headers = @{
  Accept = "application/vnd.github+json";
}
if ("${GithubToken}" -ne "") {
  $Headers['Authorization'] = $GithubToken
}

# All releases, in version order
$AllReleases = (Invoke-WebRequest -URI https://api.github.com/repos/OpenKneeboard/OpenKneeboard/releases -Headers $Headers).Content
| ConvertFrom-Json
| Sort-Object -Descending -Property { [System.Management.Automation.SemanticVersion] ($_.tag_name -replace '^v' -replace 'beta','beta.') }

function Get-Update-Version($object) {
  return $object.tag_name
}

function Write-Update-File-If-Changed($File, $NewData) {
  $OldVersion = (Get-Update-Version (Get-Content $File | ConvertFrom-Json))
  $NewVersion = (Get-Update-Version $NewData)
  Write-Host -NoNewline "[$((Get-Item $File).BaseName)] ${OldVersion} -> ${NewVersion}: "
  if ($OldVersion -eq $NewVersion) {
    Write-Host "no change."
    return
  }
  Write-Host "UPDATING"
  $NewData | ConvertTo-Json -Depth 8 -AsArray | Out-File -Encoding utf8NoBOM -FilePath $File
}

if ($Preview) {
  # Highest semver, regardless of if it's a prerelease or not
  $UpdateData = $AllReleases
  | Select-Object -First 1
  Write-Update-File-If-Changed 'preview-msi.json' $UpdateData
}

if ($Stable) {
  # Latest stable release
  $UpdateData = $AllReleases
  | Where-Object -Not -Property 'prerelease'
  | Select-Object -First 1
  Write-Update-File-If-Changed 'stable-msi.json' $UpdateData
}
