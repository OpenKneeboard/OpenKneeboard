# This script requires PowerShell 6 or above

# All releases, in version order
$AllReleases = (Invoke-WebRequest -URI https://api.github.com/repos/OpenKneeboard/OpenKneeboard/releases).Content
  | ConvertFrom-Json
  | Sort-Object -Descending -Property { [System.Management.Automation.SemanticVersion] ($_.tag_name -replace '^v') }

# Latest release, regardless of if it's a prerelease or not
$AllReleases
  | Select-Object -First 1
  | ConvertTo-Json -Depth 8 -AsArray
  | Out-File -Encoding utf8NoBOM -FilePath preview-msi.json
# As above, but only MSIX releases, for compatibility with older autoupdaters
$AllReleases
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
  | Where-Object { $_.assets.Where( {$_.name -Like "*.msix" } ).Count -gt 0 }
  | Where-Object -Not -Property 'prerelease'
  | Select-Object -First 1
  | ConvertTo-Json -Depth 8 -AsArray
  | Out-File -Encoding utf8NoBOM -FilePath stable.json
