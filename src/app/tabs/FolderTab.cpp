#include "OpenKneeboard/FolderTab.h"

#include <wincodec.h>

#include "OpenKneeboard/dprint.h"
#include "okEvents.h"

namespace OpenKneeboard {
class FolderTab::Impl final {
 public:
  struct Page {
    unsigned int Width = 0;
    unsigned int Height = 0;
    std::vector<std::byte> Pixels;

    operator bool() const {
      return !Pixels.empty();
    }
  };
  winrt::com_ptr<IWICImagingFactory> Wicf;
  std::filesystem::path Path;
  std::vector<Page> Pages = {};
  std::vector<std::filesystem::path> PagePaths = {};

  bool LoadPage(uint16_t index);
};

FolderTab::FolderTab(const wxString& title, const std::filesystem::path& path)
  : Tab(title),
    p(new Impl {
      .Wicf
      = winrt::create_instance<IWICImagingFactory>(CLSID_WICImagingFactory),
      .Path = path}) {
  Reload();
}

FolderTab::~FolderTab() {
}

void FolderTab::Reload() {
  p->Pages.clear();
  p->PagePaths.clear();
  if (!std::filesystem::is_directory(p->Path)) {
    return;
  }
  for (auto& entry: std::filesystem::recursive_directory_iterator(p->Path)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    auto wsPath = entry.path().wstring();
    if (!wxImage::CanRead(wsPath)) {
      continue;
    }
    p->PagePaths.push_back(wsPath);
  }
  p->Pages.resize(p->PagePaths.size());
  wxQueueEvent(this, new wxCommandEvent(okEVT_TAB_FULLY_REPLACED));
}

uint16_t FolderTab::GetPageCount() const {
  return p->Pages.size();
}

static bool IsValidPageIndex(uint16_t index, uint16_t count) {
  if (index < count) {
    return true;
  }

  if (index > 0) {
    dprintf("Asked for page {} >= pagecount {} in {}", index, count, __FILE__);
  }

  return false;
}

D2D1_SIZE_U FolderTab::GetPreferredPixelSize(uint16_t index) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return {};
  }

  auto& page = p->Pages.at(index);
  if (!page) {
    if (!p->LoadPage(index)) {
      return {};
    }
  }

  return {page.Width, page.Height};
}

void FolderTab::RenderPage(
  uint16_t index,
  const winrt::com_ptr<ID2D1RenderTarget>& rt,
  const D2D1_RECT_F& rect) {
  if (!IsValidPageIndex(index, GetPageCount())) {
    return;
  }

  auto& page = p->Pages.at(index);
  if (!page) {
    if (!p->LoadPage(index)) {
      return;
    }
  }

  winrt::com_ptr<ID2D1Bitmap> bmp;
  rt->CreateBitmap(
    {page.Width, page.Height},
    reinterpret_cast<const void*>(page.Pixels.data()),
    page.Width * 4,
    D2D1_BITMAP_PROPERTIES {
      {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 0, 0},
    bmp.put());

  const auto targetWidth = rect.right - rect.left;
  const auto targetHeight = rect.bottom - rect.top;
  const auto scalex = float(targetWidth) / page.Width;
  const auto scaley = float(targetHeight) / page.Height;
  const auto scale = std::min(scalex, scaley);

  const auto renderWidth = page.Width * scale;
  const auto renderHeight = page.Height * scale;

  const auto renderLeft = rect.left + ((targetWidth - renderWidth) / 2);
  const auto renderTop = rect.top + ((targetHeight - renderHeight) / 2);

  rt->DrawBitmap(
    bmp.get(),
    {renderLeft, renderTop, renderLeft + renderWidth, renderTop + renderHeight},
    1.0f,
    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

bool FolderTab::Impl::LoadPage(uint16_t index) {
  winrt::com_ptr<IWICBitmapDecoder> decoder;
  auto p = this;

  auto path = p->PagePaths.at(index).wstring();
  p->Wicf->CreateDecoderFromFilename(
    path.c_str(),
    nullptr,
    GENERIC_READ,
    WICDecodeMetadataCacheOnLoad,
    decoder.put());
  if (!decoder) {
    return false;
  }

  winrt::com_ptr<IWICBitmapFrameDecode> frame;
  decoder->GetFrame(0, frame.put());
  if (!frame) {
    return false;
  }

  winrt::com_ptr<IWICFormatConverter> converter;
  p->Wicf->CreateFormatConverter(converter.put());
  if (!converter) {
    return false;
  }
  converter->Initialize(
    frame.get(),
    GUID_WICPixelFormat32bppBGRA,
    WICBitmapDitherTypeNone,
    nullptr,
    0.0f,
    WICBitmapPaletteTypeMedianCut);

  UINT width, height;
  frame->GetSize(&width, &height);
  auto& page = p->Pages.at(index);
  page.Width = width;
  page.Height = height;
  page.Pixels.resize(width * height * 4 /* BGRA */);
  converter->CopyPixels(
    nullptr,
    width * 4,
    page.Pixels.size(),
    reinterpret_cast<BYTE*>(page.Pixels.data()));

  return true;
}

std::filesystem::path FolderTab::GetPath() const {
  return p->Path;
}

void FolderTab::SetPath(const std::filesystem::path& path) {
  if (path == p->Path) {
    return;
  }
  p->Path = path;
  Reload();
}

}// namespace OpenKneeboard
