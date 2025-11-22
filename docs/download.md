---
title: Download
permalink: /download/
nav_exclude: true
---

<style>
#dl-button {
  background-color: var(--download-button-color);
  color: white;
  border-radius: 1.2rem;
  padding: 1.2rem 1.8rem;
  width: fit-content;
  cursor: pointer;
  text-align: center;
  font-size: 1.2rem;
}
#dl-button:hover {
  background-color: var(--download-button-hover-color);
}
.dl-button-container {
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
}

#dl-button-link {
  display: inline-block;
  color: inherit;
  text-decoration: none;
  font-size: 0.7rem;
  width: fit-content;
  margin-top: 0.3rem;
  &:hover {
    text-decoration: underline;
    text-decoration-color: white;
  }
}
</style>

{% assign release = site.data.openkneeboard_latest_release %}

<div class="dl-button-container">
<div id="dl-button">&#x2193; Download OpenKneeboard</div>
<a id="dl-button-link" href="{{release.msi_url}}">{{release.msi_filename}}</a>
</div>

<script type="text/javascript">
const button = document.getElementById('dl-button');
const link = document.getElementById('dl-button-link');
button.addEventListener('click', () => link.click());
</script>

# Release notes ({{ release.tag }})

{{ release.body }}
