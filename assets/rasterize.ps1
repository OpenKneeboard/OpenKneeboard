# If you're using this as an example for something else and having trouble:
# imagemagick requires that '(' and ')' are standalone arguments (i.e. distinct
# entries in argv) - "(foo)" is invalid, but "(" "foo" ")" is valid.
#
# Here, I use `--%` to disable powershell's usual argument parsing for the rest
# of the command
magick convert `
  -background none `
  "$(Get-Location)\OpenKneeboard_Logos_Icon_Color.svg" `
  -gravity center `
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
    -gravity center `
    -trim `
    -resize $Size `
    -extent $Size `
    $FileName
}

function Create-Scaled-Png {
  param (
    $NominalSize,
    $Scale
  )
  $ActualSize = $NominalSize * $Scale / 100;
  Create-Png `
    "${ActualSize}x${ActualSize}" `
    "logo-${NominalSize}x${NominalSize}.scale-${Scale}.png"
}

function Create-Scaled-Png-Set {
  param (
    $NominalSize
  )

  Create-Png `
    "${NominalSize}x${NominalSize}" `
    "logo-${NominalSize}x${NominalSize}.png"
  Create-Scaled-Png $NominalSize 125
  Create-Scaled-Png $NominalSize 150
  Create-Scaled-Png $NominalSize 200
  Create-Scaled-Png $NominalSize 400
}


Create-Scaled-Png-Set 44
Create-Scaled-Png-Set 150

Copy-Item logo-44x44.png logo-44x44.altform-unplated.png

function Create-TargetSize-Png {
  param (
    $Size
  )
  Create-Png "${Size}x${Size}" "logo-44x44.targetsize-${Size}.png"
  Copy-Item `
    "logo-44x44.targetsize-${Size}.png" `
    "logo-44x44.targetsize-${Size}_altform-unplated.png"
}

Create-TargetSize-Png 16
Create-TargetSize-Png 24
Create-TargetSize-Png 32
Create-TargetSize-Png 48
Create-TargetSize-Png 256
