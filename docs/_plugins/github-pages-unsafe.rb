require "github-pages"

class << GitHubPages::Configuration
  alias_method :GitHubPages_effective_config, :effective_config
  def effective_config(config)
    orig = config.dup.freeze
    config = GitHubPages_effective_config(config)
    %w[safe whitelist plugins_dir].each do |key|
      config[key] = orig[key]
    end
    config
  end
end

# Pull in the plugins that GitHubPages depends on; we need to do this super-early before any of the hooks start
Jekyll.sites.each do |site|
  GitHubPages::Configuration::set(site)
end