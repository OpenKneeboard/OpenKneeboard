#pragma once

#include "okTabsList.h"

struct okTabsList::SharedState {
  std::vector<std::shared_ptr<OpenKneeboard::Tab>> Tabs;
};
