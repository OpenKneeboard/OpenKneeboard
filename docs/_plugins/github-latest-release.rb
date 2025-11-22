require 'json'
require 'net/http'
require 'uri'

module OpenKneeboardLatestRelease
  class Generator < Jekyll::Generator
    safe true

    API_URL = 'https://api.github.com/repos/openkneeboard/openkneeboard/releases/latest'.freeze

    def generate(site)
      site.data['openkneeboard_latest_release'] = fetch_latest_release(site)
    end

    private

    def fetch_latest_release(site)
      uri = URI(API_URL)
      http = Net::HTTP.new(uri.host, uri.port)
      http.use_ssl = (uri.scheme == 'https')
      req = Net::HTTP::Get.new(uri)
      req['User-Agent'] = 'OpenKneeboardDocsJekyllPlugin/1.0'
      req['Accept'] = 'application/vnd.github+json'

      token = ENV['GITHUB_TOKEN'] || ENV['GH_TOKEN']
      req['Authorization'] = "Bearer #{token}" if token && !token.empty?

      res = http.request(req)

      unless res.is_a?(Net::HTTPSuccess)
        Jekyll.logger.abort_with('OpenKneeboardLatestRelease', "GitHub API request failed: #{res.code} #{res.message}")
        return nil
      end

      json = JSON.parse(res.body)

      msi_asset = (json['assets'] || []).find do |asset|
        name = asset['name'] || ''
        name.downcase.end_with?('.msi')
      end

      unless msi_asset
        Jekyll.logger.abort_with('OpenKneeboardLatestRelease', 'No .msi asset found in latest release')
      end

      {
        'tag' => json['tag_name'],
        'name' => json['name'],
        'msi_filename' => msi_asset && msi_asset['name'],
        'msi_url' => msi_asset && msi_asset['browser_download_url'],
        'body' => json['body']
      }
    rescue StandardError => e
      Jekyll.logger.abort_with('OpenKneeboardLatestRelease', "Failed to fetch latest release: #{e.class}: #{e.message}")
      nil
    end
  end
end
