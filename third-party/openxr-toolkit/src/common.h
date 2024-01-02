// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <OpenKneeboard/tracing.h>

#include <Windows.h>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>
#include <d3d11.h>
#include <evntrace.h>
#include <wrl.h>

/*****************************/
/***** COPIED FROM pch.h *****/
/*****************************/

using Microsoft::WRL::ComPtr;
// Helpers for ComPtr manipulation.

template <typename T>
inline T* get(const ComPtr<T>& object) {
  return object.Get();
}

template <typename T>
inline T** set(ComPtr<T>& object) {
  return object.ReleaseAndGetAddressOf();
}

template <typename T>
void attach(ComPtr<T>& object, T* value) {
  object.Attach(value);
}

template <typename T>
T* detach(ComPtr<T>& object) {
  return object.Detach();
}

/*******************************/
/***** MODIFIED FROM log.h *****/
/*******************************/

#define TraceLocalActivity(activity) \
  TraceLoggingActivity<OpenKneeboard::gTraceProvider> activity;
