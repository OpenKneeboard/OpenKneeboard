#include "OpenKneeboard/RenderError.h"

namespace OpenKneeboard {

void RenderError(const wxSize& clientSize, wxDC& dc, const wxString& message) {
  auto textSize = dc.GetTextExtent(message);
  auto textOrigin = wxPoint {
    (clientSize.GetWidth() - textSize.GetWidth()) / 2,
    (clientSize.GetHeight() - textSize.GetHeight()) / 2};
  dc.SetPen(wxPen(*wxBLACK, 2));
  auto boxSize = wxSize {textSize.GetWidth() + 20, textSize.GetHeight() + 20};
  auto boxOrigin = wxPoint {
    (clientSize.GetWidth() - boxSize.GetWidth()) / 2,
    (clientSize.GetHeight() - boxSize.GetHeight()) / 2};

  dc.SetBrush(*wxGREY_BRUSH);
  dc.DrawRectangle(boxOrigin, boxSize);

  dc.SetBrush(*wxBLACK_BRUSH);
  dc.DrawText(message, textOrigin);
}
}// namespace OpenKneeboard
