module RedirectHtmlExtension
  class RedirectPage < Jekyll::Page
    def initialize(page)
      @to = page.url
      @from = page.url.gsub(%r'/$', '.html')
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
        next if page.url == '/'
        next unless page.url.end_with? '/'
        site.pages << RedirectPage.new(page)
      end
    end
  end
end