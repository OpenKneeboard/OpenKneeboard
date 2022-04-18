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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */
#pragma once

#include <d3d12.h>

#include <functional>
#include <memory>

namespace OpenKneeboard {

class ID3D12CommandQueueExecuteCommandListsHook final {
 private:
  struct Impl;
  std::unique_ptr<Impl> p;

 public:
  ID3D12CommandQueueExecuteCommandListsHook();
  ~ID3D12CommandQueueExecuteCommandListsHook();

  struct Callbacks {
    std::function<void()> onHookInstalled;
    std::function<void(
      ID3D12CommandQueue* this_,
      UINT NumCommandLists,
      ID3D12CommandList* const* ppCommandLists,
      const decltype(&ID3D12CommandQueue::ExecuteCommandLists)& next)>
      onExecuteCommandLists;
  };

  void InstallHook(const Callbacks&);
  void UninstallHook();
};

}// namespace OpenKneeboard
