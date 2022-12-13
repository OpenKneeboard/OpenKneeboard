$appx=(Get-AppxPackage -name 'FredEmmott.Self.OpenKneeboard')
if ($appx) {
	echo "Uninstalling old MSIX"
	Remove-AppxPackage $appx
}

$programData=[Environment]::GetFolderPath([System.Environment+SpecialFolder]::CommonApplicationData)
# Clean up old files...
$sandboxEscapePath="${programData}\OpenKneeboard"
if (Test-Path $sandboxEscapePath) {
  echo "Cleaning up $sandboxEscapePath..."
  Remove-Item -Recurse $sandboxEscapePath
}
echo "Checking registry"

# Clean up old registry keys
$subkey="SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit"
$roots = $("HKLM", "HKCU")
foreach ($root in $roots) {
  $key="${root}:\${subkey}"
  if (-not (Test-Path $key)) {
    echo "$key does not exist"
    continue;
  }

  echo "Checking $key"
  $layers=(Get-Item $key).Property

  foreach ($layer in $layers)
  {
    if ($layer -eq "${programData}\OpenKneeboard\OpenKneeboard-OpenXR.json")
    {
      Remove-ItemProperty -Path $key -Name $layer -Force
      echo "Removed '${layer}' from $key"
    }
  }
}
