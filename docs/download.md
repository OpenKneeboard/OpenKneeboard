---
title: Download
permalink: /download/
nav_exclude: true
---

{% assign release = site.data.openkneeboard_latest_release %}

{% include download_button.html release=release %}

# Release notes ({{ release.tag }})

{{ release.body }}
