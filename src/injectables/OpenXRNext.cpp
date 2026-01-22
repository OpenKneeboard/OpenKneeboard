// OpenKneeboard
//
// Copyright (c) 2025 Fred Emmott <fred@fredemmott.com>
//
// This program is open source; see the LICENSE file in the root of the
// OpenKneeboard repository.
#include "OpenXRNext.hpp"

namespace OpenKneeboard {

OpenXRNext::OpenXRNext(XrInstance instance, PFN_xrGetInstanceProcAddr getNext) {
  this->xrGetInstanceProcAddr = getNext;

#define DEFINE_FN_PTR(func) \
  getNext(instance, #func, reinterpret_cast<PFN_xrVoidFunction*>(&this->func));
#define DEFINE_EXT_FN_PTR(ext, func) DEFINE_FN_PTR(func)

  OPENKNEEBOARD_NEXT_OPENXR_FUNCS(DEFINE_FN_PTR, DEFINE_EXT_FN_PTR)
#undef DEFINE_FN_PTR
#undef DEFINE_EXT_FN_PTR
}

}// namespace OpenKneeboard
