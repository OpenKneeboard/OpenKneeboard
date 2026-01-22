// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include <d2d1.h>

namespace OpenKneeboard {

/** Return a system color as a Direct2D color.
 *
 * `index` is a `COLOR_*` macro from `WinUser.h`, e.g.
 * `COLOR_WINDOW`.
 */
D2D1_COLOR_F GetSystemColor(int index);

}// namespace OpenKneeboard
