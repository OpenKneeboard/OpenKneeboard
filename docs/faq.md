---
nav_order: 4
title: FAQ
---

# Frequently Asked Questions
{: .no_toc }

## Table of Contents
{: .no_toc, .text-delta }

1. TOC
{:toc}

## When should I start OpenKneeboard?

For the most part, it doesn't matter; if you find that OpenKneeboard only works reliably if you start things in a certain order, please [report a bug](https://go.openkneeboard.com/issues).

The one exception is if there is an update for OpenKneeboard: installing or updating OpenKneeboard might need you to restart any open games or OpenXR applications, so it's better to either start OpenKneeboard first in case there's an update, or leave installing updates until after you've finished the game for the day; you can install the update later by clicking 'Check for Updates' in the Help section.

## How do I use the mouse in-game?

Mice are not supported in-game; the toolbar is shown as it can be used with graphics tablets, like those made by Wacom or Huion.

Mouse support is unlikely to be added soon as it requires interfering with how the game accesses the mouse and other input devices. This is likely to cause issues with:

- anti-cheat
- the mouse and other input devices becoming unusable in the game itself

Mouse support is not planned; any updates will be on [the GitHub issue](https://github.com/OpenKneeboard/OpenKneeboard/issues/374). If there are no updates there, there is no news or ETA.

## How do I use my iPad, Microsoft Surface, other tablet computer, or phone with OpenKneeboard?

OpenKneeboard only supports graphics tablets (sometimes called 'artists tablets') such as those made by Wacom or Huion, not tablet computers or phones.

## How do I use my Xbox, Xbox clone, or other XInput controller with OpenKneeboard?

Unfortunately, Microsoft restricted Windows so that these kinds of controllers are only usable by the active Window, so they can't be used by OpenKneeboard when the game is active.

## How do I change the focal distance in VR?

As of February 2023, every consumer headset has a single focal distance for everything, which is usually between 1.3m and 2m. On some headsets this is fixed, and on other headsets, it can be adjusted by physical knobs on the headset. **No currently available headset is physically capable of adjusting focal distance for only part of what is shown**.

The discomfort happens when your eyes move between items that appear to be at different distances, but have the same focal distance; it is a physical limitation of current headsets.

Over time, most people will get used to this and it will naturally become more comfortable; you can also make things more comfortable by adjusting the rendered distance to be closer to other items in the environment, like the cockpit controls.

## How do I make OpenKneeboard start minimized?

Launch OpenKneeboard with a `--minimized` parameter; for example, by editing the shortcut:

![add `--minimized` at the end, separated by a space, after quote marks (if any)](screenshots/minimized-shortcut.png)

## How do I remove the header or footer ('borders')?

The header and footer can be disabled in Advanced Settings.

## How do I quickly adjust the VR position?

If you want things in the same place every time but they're inconsistent, recenter; **it works best to bind the same button combination for recentering OpenKneeboard and in-game**.

If you want to quickly switch between intentionally different positions (for example, if you use OpenKneeboard with multiple games or multiple aircraft), enable profiles in advanced settings, and create a different profile for each position that you want.

## How do I make landscape documents larger, like Chuck's Guides?

Open Settings -> VR -> Size, then increase "kneeboard width limit"; you may want to set it to a very high value (e.g. 10 meters), in which case the size will still be limited by the vertical height and aspect ratio.

The downside of increasing this limit is that the kneeboard is likely to interfere with the cockpit and controls.

## How do I get the best visual quality or the most readable text?

If you have a non-image source (for example, a Word document, spreadsheet, or so on), export it directly as PDF, or use 'Print to PDF'; this will get the best quality. If you only have an image, do not convert it to PDF - this will lower the quality.

For images, if the kneeboard - including OpenKneeboard's header and footer - is larger than 2048x2048 pixels or (when non-VR is active) the configured maximum on-screen size, it will be resized to fit, which will lower the quality.

To give space for header, footer, bookmarks bar etc, aim for any images to have heights and widths that are less than 1600px for VR, or 100px less than the configured maximum size for non-VR.

These guidelines are for new images; in general, resizing existing images will lower the quality compared to letting OpenKneeboard resize the image as needed. If you do choose to resize images, you will usually get the most readable text and clearest lines if you divide the number of pixels by a whole number. For example, if you have a 3000x3000px image, a half-sized version (1500x1500px) will usually be more readable than resizing to 1900x1900px, despite 1900x1900 being higher resolution.

This is because when you divide the size by a whole number, each original pixel is entirely within (part of) one pixel in the resized image. On the other hand, if you for example resize a 4x4 image to 3x3, the original second pixel will now be part of both the first and second pixels in the resized image, which will make text and lines blurry:

Original image:

![A 4x4 image with a vertical black line in the second column](resize-4x4.png)

Resized to 2x2 (half size):

![A 2x2 image - there is a grey vertical line in the first column](resize-2x2.png)

While the contrast is reduced because of merging with the pixel on the left, the line still has a solid border and is 1px wide.

Resized to 3x3 (75%):

![A 3x3 image - there is a light grey vertical line in the first column, and a dark grey in the second column](resize-3x3.png)

The vertical line now straddles two pixels, with a gradient between them; this results in blurry text and lines.

## How do I use OpenKneeboard to show content from my app?

You have three options:
- implement a custom tab type in OpenKneeboard with C++ and Direct3D 11
- provide a file or web page, and instruct users on how to configure OpenKneeboard to access that content. OpenKneeboard will automatically reload files when they are modified.
- provide a window, and instruct users on how to configure a Window Capture tab for your app

OpenKneeboard has additional integration APIs available for [web dashboards](api/web-dashboards.md).

DO NOT WRITE SOFTWARE THAT CHANGES OPENKNEEBOARD'S CONFIGURATION FILES; it is extremely likely to break users' configuration when they update OpenKneeboard.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

Feel free to *read* OpenKneeboard's configuration files from your software, but keep in mind there are no attempts at providing stability.

## How do I choose where my app's content is displayed in VR?

Instruct users on how to change their user settings to your recommendations.

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit.

DO NOT WRITE SOFTWARE THAT CHANGES OPENKNEEBOARD'S CONFIGURATION FILES; it is extremely likely to break users' configuration when they update OpenKneeboard.

## How do I use OpenKneeboard to create my own OpenXR overlay?

OpenKneeboard is a tool for users to show their content how they wish in VR, via OpenKneeboard's settings. It is not a developer toolkit. This is not a supported use-case of OpenKneeboard.

You are welcome to fork and follow the terms of the license to create a new project, however I am unable to provide any assistance to third-party projects.