# - Automatically generate redirects from /foo.html to /foo/
# - Still support `redirect_from`
module RedirectsExtension
  class RedirectPage < Jekyll::Page
    def initialize(page, from_url = nil)
      @to = page.url
      @from = from_url || page.url.gsub(%r'/$', '.html')
      @title = 'Redirect - ' + (page.data['title'] || '')

      site = page.site
      super(site, site.source, "", "🤖-generated_redirect.md")
    end

    def read_yaml(_base, _name, _opts = {})
      self.content = self.output = ""
      self.data = {
        "sitemap" => false,
        "layout" => "redirect",
        "permalink" => @from,
        "redirect_to" => @to,
        "title" => @title,
        "nav_exclude" => true,
      }
    end
  end

  class Generator < Jekyll::Generator
    safe true

    def generate(site)
      # @param [Jekyll::Page] page
      site.pages.dup.freeze.each do |page|
        if page.url != '/' && page.url.end_with?('/')
          site.pages << RedirectPage.new(page)
        end

        next unless page.data['redirect_from']

        redirects = page.data['redirect_from']
        redirects = [redirects] if redirects.is_a?(String)

        redirects.each do |from_url|
          site.pages << RedirectPage.new(page, from_url)
        end
      end
    end
  end
end