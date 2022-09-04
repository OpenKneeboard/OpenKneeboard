# Autoupdates

## Release channels

Each `.json` file is a 'release channel':

- `preview.json`: the user has opted in to this channel, or previously installed a prerelease
- `stable.json`: the user has only used stable releases

The preview channel usually should point at the latest release, regardless of if it's a prerelease or not; the stable channel should usually point at the latest stable release.

## Purpose

This decouples actually making releases from the autoupdates; in particular, this allows:

- full control over which release is used, even if there's bugs in the release selection logic
- control over timing, e.g. announcing new beta releases to Discord before pushing the autoupdate, and pushing new stable releases to the preview channel before the stable channel

## Format

These files are in the same format as https://api.github.com/repos/OpenKneeboard/OpenKneeboard/releases, documented at https://docs.github.com/en/rest/releases

## Generation

The files can be manually modified, but generation is usually desired:

1. Install PowerShell 6 or above (e.g. from the Microsoft Store)
2. run `generate.ps1`
3. check `git status` and `git diff` to check the changes
4. you may wish to revert `stable.json` to push a new release only to the `prerelease` channel
