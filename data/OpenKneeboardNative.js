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

class OpenKneeboardAPIError extends Error {
  constructor(message, apiMethodName) {
    super(message)
    this.apiMethodName = apiMethodName;
  }

  apiMethodName;
}

var OpenKneeboardNative = new class {
  async asyncRequest(name, ...args) {
    native function OKBNative_AsyncRequest();
    var json = await OKBNative_AsyncRequest(`okbjs/${name}`, JSON.stringify(args));
    var response = JSON.parse(json);
    if (response.result) {
      return response.result;
    }
    console.log(`\u26a0 OpenKneeboard API error: '${name}()' => '${response.error}'`);
    throw new OpenKneeboardAPIError(response.error, name);
  }

  get initializationData() {
    native function OKBNative_GetInitializationData();
    return JSON.parse(OKBNative_GetInitializationData());
  }
};
