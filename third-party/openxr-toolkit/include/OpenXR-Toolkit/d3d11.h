// MIT License
//
// Copyright(c) 2024 Fred Emmott
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

#include <Windows.h>

#include <d3d11.h>

namespace OpenXRToolkit::D3D11 {

// This class is not originally part of OpenXR Toolkit; it is based on
// the OpenKneeboard::D3D11::SavedState class, to expose a limited API
// over an implementation detail of OpenXR Toolkit
class SavedState final {
 public:
  SavedState(ID3D11DeviceContext*);
  ~SavedState();

  SavedState() = delete;
  SavedState(const SavedState&) = delete;
  SavedState(SavedState&&) = delete;
  SavedState& operator=(const SavedState&) = delete;
  SavedState& operator=(SavedState&&) = delete;

 private:
  struct Impl;
  Impl* m_impl {nullptr};
};

}// namespace OpenXRToolkit::D3D11