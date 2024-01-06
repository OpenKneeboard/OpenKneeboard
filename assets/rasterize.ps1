# What imagemagick gives us with default density and whitespace trimmed
# Using height as we want a square, and our input is taller than it is wide
$nativeIconHeight = 164
$nativeDpi = 96
$iconSizes = 16,32,48,64,128,256
$intermediates = @()
foreach ($size in $iconSizes) {
  $out = "icon-${size}.png"
  magick convert `
    -background none `
    -density $(($nativeDpi * $size) / $nativeIconHeight) `
    "$(Get-Location)\OpenKneeboard_Logos_Icon_Color.svg" `
    -trim `
    -gravity center `
    -extent "${size}x${size}" `
    png:$out
  $intermediates += $out
}
magick convert -background none @intermediates icon.ico
Remove-item $intermediates

# MSI dialog image with left gutter

$dialogWidth=493
$dialogHeight=312
$gutterWidth=164
$margin=16
$overlayWidth=($gutterWidth - (2 * $margin))

magick `
  -background none `
  -size ${dialogWidth}x${dialogHeight} canvas:white `
  -fill '#eee' `
  -draw "rectangle 0,0, ${gutterWidth},${dialogHeight}" `
  '(' `
  "OpenKneeboard_Logos_Flat.svg" `
  -gravity Center `
  -trim -resize $overlayWidth `
  ')' `
  -gravity NorthWest `
  -geometry +$margin+$margin `
  -compose over -composite `
  WiXUIDialog.png

# MSI banner image

$bannerWidth=$dialogWidth
$bannerHeight=58
$iconSize=40
$margin=(($bannerHeight - $iconSize) / 2)

magick `
  -background none `
  -size ${bannerWidth}x${bannerHeight} canvas:white `
  '(' `
  "OpenKneeboard_Logos_Icon_Flat.svg" `
  -gravity Center `
  -trim -resize x$iconSize `
  ')' `
  -gravity NorthEast `
  -geometry +${margin}+$margin `
  -compose over -composite `
  WiXUIBanner.png
