---
parent: Features
---

# Profiles

*This feature is disabled by default* to reduce complexity for new users.

Profiles can override almost any setting, except for OpenXR support; among others, this includes:
- bindings
- games
- tabs
- position

Profiles are frequently used to:

- save different positions for different aircraft in the same game
- show different tabs for different aircraft
- show different tabs for single player vs multiplayer, or when flying with different virtual squadrons
- use entirely different configurations for different games
- use entirely different configurations for VR vs non-VR
- use different settings for day and night missions

## Enabling profiles

Enable support for multiple profiles in OpenKneeboard Settings -> Advanced.

## Adding or deleting profiles

Click on the profile switcher in the top left of the OpenKneeboard window, then click 'edit profiles'.

Renaming is not currently supported.

## Switching profiles

You can switch profiles with:

- the profile switcher in the top left of the OpenKneeboard window
- [remote controls](../features/remote-controls.md), e.g. from Voice Attack or a StreamDeck
- the in-game menu if you are using OpenKneeboard [with a graphics tablet](./graphics-tablets.md)

### Automatically switching profiles

OpenKneeboard does not currently have built-in support for this, however it is possible to script this via [OpenKneeboard's API](../api/index.md).

## The 'Default' profile

The Default profile is used as the base settings for all other profiles - if you do not change a setting in another profile, the current setting from the Default profile will be used.

For example, if you change input bindings in the Default profile, this will affect all other profiles you have created, except for ones that already have bindings for that device.
