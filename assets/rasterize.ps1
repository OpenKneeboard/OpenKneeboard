# If you're using this as an example for something else and having trouble:
# imagemagick requires that '(' and ')' are standalone arguments (i.e. distinct
# entries in argv) - "(foo)" is invalid, but "(" "foo" ")" is valid.
#
# Here, I use `--%` to disable powershell's usual argument parsing for the rest
# of the command
magick convert `
  -background none `
  "$(Get-Location)\OpenKneeboard_Logos_Icon_Color.svg" `
  -trim `
  --% ( -clone 0 -resize 16x16 -extent 16x16 ) ( -clone 0 -resize 32x32 -extent 32x32 ) ( -clone 0 -resize 48x48 -extent 48x48 ) ( -clone 0 -resize 64x64 -extent 64x64 ) ( -clone 0 -resize 128x128 -extent 128x128 ) ( -clone 0 -resize 256x256 -extent 256x256 ) icon.ico

function Create-Png {
  param (
    $Size,
    $FileName
  )

  magick convert `
    -background none `
    "$(Get-Location)\OpenKneeboard_Logos_Icon_Color.svg" `
    -trim `
    -resize $Size `
    -extent $Size `
    $FileName
}

# See:
# - https://docs.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-uap-visualelements
# - https://docs.microsoft.com/en-us/previous-versions/windows/apps/hh831183(v=win.10)

Create-Png 150x150 logo-150x150.scale-100.png
Create-Png 375x375 logo-150x150.scale-250.png

Create-Png 44x44 logo-44x44.scale-100.png
Create-Png 110x110 logo-44x44.scale-250.png

Create-Png 300x300 logo-300x300.png
