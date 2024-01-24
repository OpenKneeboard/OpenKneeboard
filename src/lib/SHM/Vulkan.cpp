/*
 * OpenKneeboard
 *
 * Copyright (C) 2022-present Fred Emmott <fred@fredemmott.com>
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

#include <OpenKneeboard/SHM/Vulkan.h>

#include <OpenKneeboard/dprint.h>
#include <OpenKneeboard/tracing.h>

namespace OpenKneeboard::SHM::Vulkan {

CachedReader::CachedReader(ConsumerKind consumerKind)
  : SHM::CachedReader(this, consumerKind) {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D::CachedReader::CachedReader()");
}

CachedReader::~CachedReader() {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D::CachedReader::~CachedReader()");
}

void CachedReader::Copy(
  uint8_t swapchainIndex,
  HANDLE sourceTexture,
  IPCClientTexture* destinationTexture,
  HANDLE fence,
  uint64_t fenceValueIn,
  uint64_t fenceValueOut) noexcept {
  OPENKNEEBOARD_TraceLoggingScope("SHM::D3D::CachedReader::Copy()");
  // TODO
}

std::shared_ptr<SHM::IPCClientTexture> CachedReader::CreateIPCClientTexture(
  uint8_t swapchainIndex) noexcept {
  OPENKNEEBOARD_TraceLoggingScope(
    "SHM::D3D::CachedReader::CreateIPCClientTexture()");
  return std::make_shared<Texture>();
}

};// namespace OpenKneeboard::SHM::Vulkan