---
nav_order: 4
title: FAQ
---

# Frequently Asked Questions
{: .no_toc }

1. TOC
{:toc}

## When should I start OpenKneeboard?

For the most part, it doesn't matter; if you find that OpenKneeboard only works reliably if you start things in a certain order, please [report a bug](https://go.openkneeboard.com/issues).

The one exception is if there is an update for OpenKneeboard: installing or updating OpenKneeboard might need you to restart any open games or OpenXR applications, so it's better to either start OpenKneeboard first in case there's an update, or leave installing updates until after you've finished the game for the day; you can install the update later by clicking 'Check for Updates' in the Help section.

## How do I use the mouse in-game?

Mice are not supported in-game; the toolbar is shown as it can be used with graphics tablets, like those made by Wacom or Huion.

## How do I use my iPad, Microsoft Surface, other tablet computer, or phone with OpenKneeboard?

OpenKneeboard only supports graphics tablets (sometimes called 'artists tablets') such as those made by Wacom or Huion, not tablet computers or phones.

## How do I use my Xbox, Xbox clone, or other XInput controller with OpenKneeboard?

Unfortunately, Microsoft restricted Windows so that these kinds of controllers are only usable by the active Window, so they can't be used by OpenKneeboard when the game is active.

## How do I change the focal distance in VR?

As of February 2023, every consumer headset has a single focal distance for everything, which is usually between 1.3m and 2m. On some headsets this is fixed, and on other headsets, it can be adjusted by physical knobs on the headset. **No currently available headset is physically capable of adjusting focal distance for only part of what is shown**.

The discomfort happens when your eyes move between items that appear to be at different distances, but have the same focal distance; it is a physical limitation of current headsets.

Over time, most people will get used to this and it will naturally become more comfortable; you can also make things more comfortable by adjusting the rendered distance to be closer to other items in the environment, like the cockpit controls.

## How do I make OpenKneeboard start minimized?

v1.3 and below: this is not supported

v1.4 and above: launch OpenKneeboard with a `--minimized` parameter; for example, by editing the shortcut:

![add `--minimized` at the end, separated by a space, after quote marks (if any)](screenshots/minimized-shortcut.png)

## How do I remove the header or footer ('borders')?

v1.3 and below: this is not supported

v1.4: the header and footer can be disabled in Advanced Settings.

## How do I quickly adjust the VR position?

If you want things in the same place every time but they're inconsistent, recenter; **it works best to bind the same button combination for recentering OpenKneeboard and in-game**.

If you want to quickly switch between intentionally different positions (for example, if you use OpenKneeboard with multiple games or multiple aircraft), enable profiles in advanced settings, and create a different profile for each position that you want.

## How do I make landscape documents larger, like Chuck's Guides?

Open Settings -> VR -> Size, then increase "kneeboard width limit"; you may want to set it to a very high value (e.g. 10 meters), in which case the size will still be limited by the vertical height and aspect ratio.

The downside of increasing this limit is that the kneeboard is likely to interfere with the cockpit and controls.