---
parent: FAQ
title: Feature Requests FAQ
---
# Feature Requests FAQ
{: .no_toc }

## Table of Contents
{: .no_toc, .text-delta }

1. TOC
{:toc}

## Can you add my feature idea?

OpenKneeboard is a tool for displaying content in a world-locked position, and navigating it with pages and tabs. I will not accept feature requests or contributions that do not fit this model; for example, **do not ask** for the following features - the answer is no:

- features related to mixed reality
- attaching views/content to something that moves, such as your head, hand or controllers
- 'workaround' feature requests for the above, e.g. 'recentering every frame'

For other feature requests, search [GitHub Issues](https://github.com/OpenKneeboard/OpenKneeboard/issues?q=is%3Aissue); if a request for the specific feature already exists and is open, perhaps. If it exists and is closed, probably not - read the comments in the issue for details. If your request is not **exactly** the same, please open a second issue - don't add it as a comment to any existing ones.

## Why won't you add features for mixed reality?

OpenKneeboard is designed to solve specific problems, not to be a general-purpose tool. While for many people OpenKneeboard is the best current option for mixed reality, it is still not a *good* option, and can not become one without becoming worse at the problems it is intended to solve and drastically increasing the amount of time required for maintenance and testing.

OpenKneeboard has *no* mixed reality features; feature requests for mixed reality are best directed at whatever software you are using to add them. If you are a developer, development time would be much better spent on creating a dedicated OpenXR API layer and configuration tool, without the majority of OpenKneeboard's features (and corresponding complexity), but with support for:

- Chroma key composition layers (e.g. for virtual desktop)
- Modifying the projection composition layer to directly include passthrough regions via `XR_ENVIRONMENT_BLEND_MODE_ADDITIVE` and `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND` (e.g. for Varjo)
- webcam capture composition layers for DIY builds
- Vendor-specific extensions such as `XR_FB_passthrough` (link developer mode, Vive Cosmos, and Pico 4), and `XR_HTC_passthrough` (Vive Focus)
- Curved overlay regions via `XR_KHR_composition_layer_cylinder` 
- True motion-compensated headlocked layers via `XR_REFERENCE_SPACE_TYPE_VIEW`
- Composition layers locked to controllers or hand tracking
- Spatial anchors for tying 'portals' to real-world objects

A dedicated tool could be made simultaneously much easier to use and much more powerful than bolting on special-purpose features to OpenKneeboard, and adding these features to OpenKneeboard would drastically increase it's complexity, both for users and developers.

## How do I show support for a feature request?

Add a thumbs up to the first comment on the GitHub issue. Comments equivalent to "this is important to me too" do not add anything.

## When will my feature be added?

I work on OpenKneeboard when it is most fun or interesting way for me to spend my free time; similarly, your feature request will be added whenever it is the most fun or interesting way for me to spend my free time, or someone other than me implements and contributes it.

If your feature request comes from an established US business in the VR, simulation, aviation, defense, or other relevant industries, I may be available for contract work; please [contact me](mailto:openkneeboard-contractwork@fred.fredemmott.com) me for details. I do not accept contracts for work from individuals, defense companies outside the US, or businesses that do not have an established presence.

Outside of contract work, I do not do estimates of time or probability; requests for estimates will be ignored, and repeat requests will be considered spam.

## Is there any update on my feature request?

Any updates are on the GitHub issue; if there is no update on the GitHub issue, there is no update, and no need to ask for one; requests for updates will be ignored, and repeat requests will be considered spam.

## How likely is my feature request to be implemented?

I don't do estimates of time or probability.

## GitHub shows my feature request was completed; when will it be available?

Most likely in the next release where the first or second parts of the version number change. For example, if the current version is `v1.8.4`, feature requests will mostly be in `v1.9.0`, not `v1.8.5`. Bugfixes are will usually be in the next release; continuing the previous example, this would be `v1.8.5`.

There is no release schedule, and I do not estimate timeframes. Do not ask.

## Can you integrate more with some app?

I generally do not spend time on app integrations unless I use the app personally, however contributions are welcome, and I may work on specific feature requests that improve integration. Feature request issues to integrate more with a specific app will most likely be closed without investigation, unless they are simply a reference list of specific feature requests or bug reports related to the app.

App developers can either directly contribute an integration, e.g. a custom tab type implemented in C++, or use the [documented APIs](../api/index.md). There is also a separate [third-party developer FAQ](./third-party-developers.md).