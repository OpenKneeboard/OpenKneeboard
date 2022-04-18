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
#include "ScopedRWX.h"

#include <Windows.h>

namespace OpenKneeboard {

struct ScopedRWX::Impl {
  MEMORY_BASIC_INFORMATION mMBI;
  DWORD mOldProtection;
};

ScopedRWX::ScopedRWX(void* addr) : p(std::make_unique<Impl>()) {
  VirtualQuery(addr, &p->mMBI, sizeof(p->mMBI));
  VirtualProtect(
    p->mMBI.BaseAddress,
    p->mMBI.RegionSize,
    PAGE_EXECUTE_READWRITE,
    &p->mOldProtection);
}

ScopedRWX::~ScopedRWX() {
  DWORD rwx;
  VirtualProtect(
    p->mMBI.BaseAddress, p->mMBI.RegionSize, p->mOldProtection, &rwx);
}

}// namespace OpenKneeboard
