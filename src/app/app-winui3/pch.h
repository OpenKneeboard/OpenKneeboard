// CLion pulls in this header at configure-time when tyring to identify
// the compiler, so several of the things we pull in (including
// both winrt-generated files and ExternalProject_Add dependencies) are
// unavailable. Check for a sentinel file to figure out whether or not we're
// actually building
#if __has_include("openkneeboard-build-time-marker.hpp") && !defined(OPENKNEEBOARD_PCH_H)
#define OPENKNEEBOARD_PCH_H

#define NOMINMAX 1

// clang-format off
#include <shims/winrt/base.h>
#include <hstring.h>
#include <restrictederrorinfo.h>
#include <windows.h>
// clang-format on

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include "Globals.h"

// clang-format off
#include <combaseapi.h>
#include <ctxtcall.h>
// clang-format on

#include <shims/winrt/base.h>

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <OpenKneeboard/bindline.hpp>
#include <OpenKneeboard/task.hpp>
#include <OpenKneeboard/weak_refs.hpp>

#include <wil/cppwinrt_helpers.h>

namespace winrt::OpenKneeboardApp {
using namespace ::OpenKneeboard::task_ns;
}

#endif