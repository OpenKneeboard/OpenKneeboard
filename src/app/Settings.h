#pragma once

#include <nlohmann/json.hpp>

struct Settings final {
  uint32_t Version = 1;

  nlohmann::json DirectInput;
  nlohmann::json Games;
  nlohmann::json Tabs;

  static Settings Load();
  void Save();
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Settings, Version, DirectInput, Games, Tabs);
