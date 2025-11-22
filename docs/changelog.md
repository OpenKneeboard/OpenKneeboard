---
nav_order: 9998
---

# Release History

Only the latest of OpenKneeboard is supported, and downgrading is not supported. If you install outdated, development, or test versions, you may need to reset your settings with [the Fresh Start tool](https://github.com/OpenKneeboard/Fresh-Start/releases/latest).

<label class="toggle-prereleases">
  <input type="checkbox" id="togglePrereleases"> Show prerelease versions
</label>

{% for release in site.data.openkneeboard_releases %}

<details id="{{ release.tag }}" {% if release.is_latest %}open{% endif %} {% if release.is_prerelease %}class="prerelease"{% endif %}>
  <summary>
    {{ release.tag }}
    {% if release.is_latest %}<span class="badge badge-success">Latest</span>{% endif %}
    {% if release.is_prerelease %}<span class="badge badge-warning">Prerelease</span>{% endif %}
    <span
      class="release-published-at"
      title="{{ release.published_at }}"
    >{{ release.published_at| date: "%Y-%m-%d" }}</span>
    <span class="release-copy-link" data-release-tag="{{ release.tag }}">copy link</span>
  </summary>
  {{ release.body | emojify | markdownify }}

{% if release.is_latest %}
{% include download_button.html release=release %}
{% else %}
<div>
  {% if release.is_prerelease %}
    <span class="badge badge-warning" title="This version is only intended for developers and testers; you should not install it even if you want an older release">Prerelease</span>
  {% endif %}
  <span class="badge badge-warning" title="This version is outdated and unsupported. You should install the latest version instead">Outdated</span>
  <a href="{{release.installer_url}}" class="release-download" data-release-tag="{{release.tag}}" data-release-is-prerelease="{{release.is_prerelease}}">{{release.installer_filename}}</a>
</div>
{% endif %}
</details>

{% endfor %}

<script>
  document.addEventListener('DOMContentLoaded', function() {
    document.getElementById('togglePrereleases').addEventListener('change', function(e) {
      document.body.classList.toggle('with-prerelease-details', e.target.checked);
    });

    if (window.location.hash) {
      const id = window.location.hash.substring(1);
      const details = document.getElementById(id);
      if (details) {
        details.open = true;
        if (details.classList.contains('prerelease')) {
          document.getElementById('togglePrereleases').checked = true;
          document.body.classList.add('with-prerelease-details');
          setTimeout(() => details.scrollIntoView({ behavior: 'instant', block: 'start'}), 0);
        }
      }
    }

    document.querySelectorAll('.release-copy-link').forEach(element => {
      element.addEventListener('click', function(e) {
        e.preventDefault();
        const releaseTag = this.getAttribute('data-release-tag');
        const url = `${window.location.origin}${window.location.pathname}#${releaseTag}`;
        navigator.clipboard.writeText(url);
      });
    });

    document.querySelectorAll('.release-download').forEach(element => {
      element.addEventListener('click', function(e) {
        const isPrerelease = this.getAttribute('data-release-is-prerelease') === 'true';
        const version = this.getAttribute('data-release-tag');
        if (isPrerelease) {
          if (!confirm(`${version} is an unfinished version only suitable for developers and testers. You should not use it even if you are looking for an older version. Are you sure you want to download it?`)) {
            e.preventDefault();
          }
        } else {
          if (!confirm(`${version} is outdated and unsupported; you should install the latest version instead. Are you sure you want to download it?`)) {
            e.preventDefault();
          }
        }
      });
    });
  });
</script>

