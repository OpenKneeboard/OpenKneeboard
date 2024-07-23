$Out = "Test.OpenKneeboardPlugin";
$Zip = "$Out.zip"
Remove-Item $Out
if (Test-Path $Zip) {
    Remove-Item $Zip
}
Compress-Archive v1.json -DestinationPath $Zip
Rename-Item $Zip $Out