require 'json'
require 'net/http'
require 'uri'

module GithubReleases
  class Generator < Jekyll::Generator
    safe true

    API_BASE = 'https://api.github.com/repos/openkneeboard/openkneeboard'.freeze

    def generate(site)
      latest = fetch_latest_release(site)
      site.data['openkneeboard_latest_release'] = latest if latest

      releases = fetch_all_releases(site, latest_tag: latest && latest['tag'])
      site.data['openkneeboard_releases'] = releases if releases
    end

    private

    def http_get(uri)
      http = Net::HTTP.new(uri.host, uri.port)
      http.use_ssl = (uri.scheme == 'https')
      req = Net::HTTP::Get.new(uri)
      req['User-Agent'] = 'OpenKneeboardDocsJekyllPlugin/1.0'
      req['Accept'] = 'application/vnd.github+json'
      token = ENV['GITHUB_TOKEN'] || ENV['GH_TOKEN']
      req['Authorization'] = "Bearer #{token}" if token && !token.empty?
      http.request(req)
    end

    def fetch_latest_release(site)
      uri = URI("#{API_BASE}/releases/latest")
      res = http_get(uri)

      unless res.is_a?(Net::HTTPSuccess)
        Jekyll.logger.abort_with('OpenKneeboardLatestRelease', "GitHub API request failed: #{res.code} #{res.message}")
        return nil
      end

      json = JSON.parse(res.body)

      installer_asset = find_installer_asset(json)

      unless installer_asset
        Jekyll.logger.abort_with('OpenKneeboardLatestRelease', 'No .msi asset found in latest release')
      end

      {
        'tag' => json['tag_name'],
        'name' => json['name'],
        'installer_filename' => installer_asset && installer_asset['name'],
        'installer_url' => installer_asset && installer_asset['browser_download_url'],
        'body' => json['body'],
        'is_latest' => true,
        'is_prerelease' => false,
        'published_at' => json['published_at'],
      }
    rescue StandardError => e
      Jekyll.logger.abort_with('OpenKneeboardLatestRelease', "Failed to fetch latest release: #{e.class}: #{e.message}")
      nil
    end

    def fetch_all_releases(site, latest_tag: nil)
      per_page = 100
      page = 1
      all = []

      loop do
        uri = URI("#{API_BASE}/releases?per_page=#{per_page}&page=#{page}")
        res = http_get(uri)
        unless res.is_a?(Net::HTTPSuccess)
          Jekyll.logger.abort_with('OpenKneeboardLatestRelease', "GitHub API request failed (page #{page}): #{res.code} #{res.message}")
          return []
        end
        arr = JSON.parse(res.body)
        break if !arr.is_a?(Array) || arr.empty?

        arr.each do |r|
          next if r['draft'] # skip drafts

          msi = find_installer_asset(r)
          all << {
            'tag' => r['tag_name'],
            'name' => r['name'],
            'installer_filename' => msi && msi['name'],
            'installer_url' => msi && msi['browser_download_url'],
            'body' => r['body'],
            'is_latest' => (!latest_tag.nil? && r['tag_name'] == latest_tag),
            'is_prerelease' => !!r['prerelease'],
            'published_at' => r['published_at'],
          }
        end

        break if arr.length < per_page
        page += 1
      end

      all
    rescue StandardError => e
      Jekyll.logger.abort_with('OpenKneeboardLatestRelease', "Failed to fetch releases: #{e.class}: #{e.message}")
      []
    end

    def find_installer_asset(release_json)
      (release_json['assets'] || []).find do |asset|
        name = (asset['name'] || '').downcase
        name.end_with?('.msi') || name.end_with?('.msix')
      end
    end
  end
end
