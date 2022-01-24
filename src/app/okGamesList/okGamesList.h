/*
 * OpenKneeboard
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#pragma once

#include <OpenKneeboard/Game.h>

#include "GameInstance.h"
#include "okConfigurableComponent.h"

class okGameInjectorThread;

class okGamesList final : public okConfigurableComponent {
 private:
  class SettingsUI;
  std::vector<std::shared_ptr<OpenKneeboard::Game>> mGames;
  std::vector<OpenKneeboard::GameInstance> mInstances;
  okGameInjectorThread* mInjector = nullptr;

 private:
  void LoadDefaultSettings();
  void LoadSettings(const nlohmann::json&);

 public:
  okGamesList() = delete;
  okGamesList(const nlohmann::json& config);
  virtual ~okGamesList();
  virtual wxWindow* GetSettingsUI(wxWindow* parent) override;
  virtual nlohmann::json GetSettings() const override;
};
