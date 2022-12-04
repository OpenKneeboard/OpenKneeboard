$appx=(Get-AppxPackage -name 'FredEmmott.Self.OpenKneeboard')
if (!$appx) {
  return;
}
Remove-AppxPackage $appx
