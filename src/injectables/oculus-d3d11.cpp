// Copyright 2016 Patrick Mours.All rights reserved.
// Copyright(c) 2017 Advanced Micro Devices, Inc. All rights reserved.
// Copyright(c) 2021-present Fred Emmott. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// https://github.com/GPUOpen-Tools/ocat used as reference; copyright notice
// above reflects copyright notices in source material.

#include <OVR_CAPI_D3D.H>
#include <TlHelp32.h>
#include <Unknwn.h>
#include <d3d11.h>
#include <detours.h>
#include <fmt/format.h>
#include <windows.h>
#include <winrt/base.h>

#include <filesystem>
#include <functional>
#include <vector>

#include "d3d11-offsets.h"

static bool g_hooked_DX = false;
static bool g_initialized = false;
static ovrTextureSwapChain g_kneeboardSwapChain;
static winrt::com_ptr<ID3D11Device> g_d3dDevice;
static std::vector<winrt::com_ptr<ID3D11RenderTargetView>>
	g_kneeboardRenderTargets;

static const int TEXTURE_WIDTH = 400;
static const int TEXTURE_HEIGHT = 1200;

// Not documented, but defined in libovrrt64_1.dll
// Got signature from Revive, and it matches ovr_SubmitFrame
//
// Some games (e.g. DCS World) call this directly
OVR_PUBLIC_FUNCTION(ovrResult)
ovr_SubmitFrame2(
	ovrSession session,
	long long frameIndex,
	const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const* const* layerPtrList,
	unsigned int layerCount);

#define HOOKED_OVR_FUNCS \
	/* Our main hook: render the kneeboard */ \
	IT(ovr_EndFrame) \
	IT(ovr_SubmitFrame) \
	IT(ovr_SubmitFrame2)

static_assert(std::same_as<decltype(ovr_EndFrame), decltype(ovr_SubmitFrame)>);
static_assert(std::same_as<decltype(ovr_EndFrame), decltype(ovr_SubmitFrame2)>);

#define HOOKED_FUNCS HOOKED_OVR_FUNCS

#define UNHOOKED_OVR_FUNCS \
	/* We need the versions from the process, not whatever we had handy when \ \ \
	 * \ \ \ building */ \
	IT(ovr_CreateTextureSwapChainDX) \
	IT(ovr_GetTextureSwapChainLength) \
	IT(ovr_GetTextureSwapChainBufferDX) \
	IT(ovr_GetTextureSwapChainCurrentIndex) \
	IT(ovr_CommitTextureSwapChain)

#define REAL_FUNCS HOOKED_FUNCS UNHOOKED_OVR_FUNCS

#define IT(x) decltype(&x) Real_##x = nullptr;
REAL_FUNCS
#undef IT

// methods
decltype(&IDXGISwapChain::Present) Real_IDXGISwapChain_Present = nullptr;

template <typename... T>
void dprint(fmt::format_string<T...> fmt, T&&... args) {
	auto str = fmt::format(fmt, std::forward<T>(args)...);
	OutputDebugStringA(str.c_str());
}

void DetourUpdateAllThreads() {
	auto snapshot
		= CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, GetCurrentProcessId());
	THREADENTRY32 thread;
	thread.dwSize = sizeof(thread);

	DetourUpdateThread(GetCurrentThread());
	auto myProc = GetCurrentProcessId();
	auto myThread = GetCurrentThreadId();
	if (!Thread32First(snapshot, &thread)) {
		dprint("Failed to find first thread");
		return;
	}

	do {
		if (!thread.th32ThreadID) {
			continue;
		}
		if (thread.th32OwnerProcessID != myProc) {
			// CreateToolhelp32Snapshot takes a process ID, but ignores it
			continue;
		}
		if (thread.th32ThreadID == myThread) {
			continue;
		}

		auto th = OpenThread(THREAD_ALL_ACCESS, false, thread.th32ThreadID);
		DetourUpdateThread(th);
	} while (Thread32Next(snapshot, &thread));
}

class HookedMethods final {
 public:
	HRESULT __stdcall Hooked_IDXGISwapChain_Present(
		UINT SyncInterval,
		UINT Flags) {
		auto _this = reinterpret_cast<IDXGISwapChain*>(this);
		if (!g_d3dDevice) {
			_this->GetDevice(IID_PPV_ARGS(&g_d3dDevice));
			dprint(
				"Got device at {:#010x} from {}",
				(intptr_t)g_d3dDevice.get(),
				__FUNCTION__);
		}
		return std::invoke(Real_IDXGISwapChain_Present, _this, SyncInterval, Flags);
	}
};

template <class T>
bool find_function(T** funcPtrPtr, const char* lib, const char* name) {
	*funcPtrPtr = reinterpret_cast<T*>(DetourFindFunction(lib, name));

	if (*funcPtrPtr) {
		dprint(
			"- Found {} at {:#010x}", name, reinterpret_cast<intptr_t>(*funcPtrPtr));
		return true;
	}

	dprint(" - Failed to find {}", name);
	return false;
}

template <class T>
bool find_libovr_function(T** funcPtrPtr, const char* name) {
	return find_function(funcPtrPtr, "LibOVRRT64_1.dll", name);
}

static void hook_IDXGISwapChain_Present() {
	if (g_hooked_DX) {
		return;
	}
	g_hooked_DX = true;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Windowed = TRUE;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;

	winrt::com_ptr<IDXGISwapChain> swapchain;
	winrt::com_ptr<ID3D11Device> device;

	decltype(&D3D11CreateDeviceAndSwapChain) factory = nullptr;
	factory = reinterpret_cast<decltype(factory)>(
		DetourFindFunction("d3d11.dll", "D3D11CreateDeviceAndSwapChain"));
	dprint("Creating temporary device and swap chain");
	auto ret = factory(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		D3D11_CREATE_DEVICE_DEBUG,
		&level,
		1,
		D3D11_SDK_VERSION,
		&sd,
		swapchain.put(),
		device.put(),
		nullptr,
		nullptr);
	dprint(" - got a temporary device at {:#010x}", (intptr_t)device.get());
	dprint(" - got a temporary SwapChain at {:#010x}", (intptr_t)swapchain.get());

	auto fpp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
	*fpp = VTable_Lookup_IDXGISwapChain_Present(swapchain.get());
	dprint(" - found IDXGISwapChain::Present at {:#010x}", (intptr_t)*fpp);
	auto mfp = &HookedMethods::Hooked_IDXGISwapChain_Present;
	dprint(
		"Hooking &{:#010x} to {:#010x}",
		(intptr_t)*fpp,
		(intptr_t) * (reinterpret_cast<void**>(&mfp)));
	DetourTransactionBegin();
	DetourUpdateAllThreads();
	auto err = DetourAttach(fpp, *(reinterpret_cast<void**>(&mfp)));
	if (err == 0) {
		dprint(" - hooked IDXGISwapChain::Present().");
	} else {
		dprint(" - failed to hook IDXGISwapChain::Present(): {}", err);
	}
	DetourTransactionCommit();
}

static void unhook_IDXGISwapChain_Present() {
	dprint("{}", __FUNCTION__);
	auto ffp = reinterpret_cast<void**>(&Real_IDXGISwapChain_Present);
	auto mfp = &HookedMethods::Hooked_IDXGISwapChain_Present;
	DetourTransactionBegin();
	DetourUpdateAllThreads();
	auto err = DetourDetach(ffp, *(reinterpret_cast<void**>(&mfp)));
	dprint("{} detach {}", __FUNCTION__, err);
	err = DetourTransactionCommit();
	dprint("{} commit {}", __FUNCTION__, err);
}

static bool KneeboardRenderInit(ovrSession session) {
	if (g_initialized) {
		return true;
	}

	if (!g_d3dDevice) {
		hook_IDXGISwapChain_Present();
		// Initialized by Hooked_ovrCreateTextureSwapChainDX
		return false;
	}
	unhook_IDXGISwapChain_Present();

	ovrTextureSwapChainDesc kneeboardSCD = {
		.Type = ovrTexture_2D,
		.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB,
		.ArraySize = 1,
		.Width = TEXTURE_WIDTH,
		.Height = TEXTURE_HEIGHT,
		.MipLevels = 1,
		.SampleCount = 1,
		.StaticImage = false,
		.MiscFlags = ovrTextureMisc_AutoGenerateMips,
		.BindFlags = ovrTextureBind_DX_RenderTarget,
	};

	auto ret = Real_ovr_CreateTextureSwapChainDX(
		session, g_d3dDevice.get(), &kneeboardSCD, &g_kneeboardSwapChain);
	dprint("CreateSwapChain ret: {}", ret);

	int length = -1;
	Real_ovr_GetTextureSwapChainLength(session, g_kneeboardSwapChain, &length);
	if (length < 1) {
		dprint(" - invalid swap chain length ({})", length);
		return false;
	}
	dprint(" - got swap chain length {}", length);

	g_kneeboardRenderTargets.clear();
	g_kneeboardRenderTargets.resize(length);
	for (int i = 0; i < length; ++i) {
		ID3D11Texture2D* texture;// todo: smart ptr?
		auto res = Real_ovr_GetTextureSwapChainBufferDX(
			session, g_kneeboardSwapChain, i, IID_PPV_ARGS(&texture));
		if (res != 0) {
			dprint(" - failed to get swap chain buffer: {}", res);
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC rtvd = {
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {.MipSlice = 0}};

		auto hr = g_d3dDevice->CreateRenderTargetView(
			texture, &rtvd, g_kneeboardRenderTargets.at(i).put());
		if (FAILED(hr)) {
			dprint(" - failed to create render target view");
			return false;
		}
	}

	dprint("{} completed", __FUNCTION__);

	g_initialized = true;
	return true;
}

static bool KneeboardRender(ovrSession session) {
	if (!KneeboardRenderInit(session)) {
		return false;
	}
	static bool firstRun = true;
	if (firstRun) {
		dprint(
			"{} with d3ddevice at {:#010x}",
			__FUNCTION__,
			(intptr_t)g_d3dDevice.get());
	}

	int index = -1;
	Real_ovr_GetTextureSwapChainCurrentIndex(
		session, g_kneeboardSwapChain, &index);
	if (index < 0) {
		dprint(" - invalid swap chain index ({})", index);
		return false;
	}

	winrt::com_ptr<ID3D11DeviceContext> context;
	g_d3dDevice->GetImmediateContext(context.put());
	if (!context) {
		dprint(" - invalid context");
		return false;
	}
	auto target = g_kneeboardRenderTargets.at(index).get();
	context->OMSetRenderTargets(1, &target, nullptr);

	const float red[] = {1.0f, 0.0f, 0.0f, 0.5f};
	context->ClearRenderTargetView(target, red);

	auto ret = Real_ovr_CommitTextureSwapChain(session, g_kneeboardSwapChain);
	if (ret) {
		dprint("Commit failed with {}", ret);
	}
	if (firstRun) {
		dprint(" - success");
		firstRun = false;
	}

	return true;
}

static ovrResult EndFrame_Hook_Impl(
	decltype(&ovr_EndFrame) nextImpl,
	ovrSession session,
	long long frameIndex,
	const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const* const* layerPtrList,
	unsigned int layerCount) {
	if (!KneeboardRender(session)) {
		return nextImpl(
			session, frameIndex, viewScaleDesc, layerPtrList, layerCount);
	}

	ovrLayerQuad kneeboardLayer = {};
	kneeboardLayer.Header.Type = ovrLayerType_Quad;
	kneeboardLayer.Header.Flags |= ovrLayerFlag_HeadLocked;
	kneeboardLayer.ColorTexture = g_kneeboardSwapChain;
	// all units are real-world meters
	// 15cm down from nose, 50cm away
	kneeboardLayer.QuadPoseCenter.Position = {.x = 0.0f, .y = -0.15f, .z = -0.5f};
	kneeboardLayer.QuadPoseCenter.Orientation
		= {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};
	// on knee
	// kneeboardLayer.QuadPoseCenter.Position = {.x = 0.0f, .y = 0.5f, .z =
	// -0.25f};
	// kneeboardLayer.QuadPoseCenter.Orientation
	//  = {.x = 0.7071f, .y = 0.0f, .z = 0.0f, .w = 0.7071f};
	kneeboardLayer.QuadSize = {.x = 0.2f, .y = 0.3f};
	kneeboardLayer.Viewport.Pos = {.x = 0, .y = 0};
	kneeboardLayer.Viewport.Size = {.w = TEXTURE_WIDTH, .h = TEXTURE_HEIGHT};

	std::vector<const ovrLayerHeader*> newLayers;
	if (layerCount == 0) {
		newLayers.push_back(&kneeboardLayer.Header);
	} else if (layerCount < ovrMaxLayerCount) {
		newLayers = {&layerPtrList[0], &layerPtrList[layerCount]};
		newLayers.push_back(&kneeboardLayer.Header);
	} else {
		for (unsigned int i = 0; i < layerCount; ++i) {
			if (layerPtrList[i]) {
				newLayers.push_back(layerPtrList[i]);
			}
		}

		if (newLayers.size() < ovrMaxLayerCount) {
			newLayers.push_back(&kneeboardLayer.Header);
		} else {
			dprint("Already at ovrMaxLayerCount without adding our layer");
		}
	}

	return nextImpl(
		session,
		frameIndex,
		viewScaleDesc,
		newLayers.data(),
		static_cast<unsigned int>(newLayers.size()));
}

OVR_PUBLIC_FUNCTION(ovrResult)
Hooked_ovr_EndFrame(
	ovrSession session,
	long long frameIndex,
	const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const* const* layerPtrList,
	unsigned int layerCount) {
	return EndFrame_Hook_Impl(
		Real_ovr_EndFrame,
		session,
		frameIndex,
		viewScaleDesc,
		layerPtrList,
		layerCount);
}

OVR_PUBLIC_FUNCTION(ovrResult)
Hooked_ovr_SubmitFrame(
	ovrSession session,
	long long frameIndex,
	const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const* const* layerPtrList,
	unsigned int layerCount) {
	return EndFrame_Hook_Impl(
		Real_ovr_SubmitFrame,
		session,
		frameIndex,
		viewScaleDesc,
		layerPtrList,
		layerCount);
}

OVR_PUBLIC_FUNCTION(ovrResult)
Hooked_ovr_SubmitFrame2(
	ovrSession session,
	long long frameIndex,
	const ovrViewScaleDesc* viewScaleDesc,
	ovrLayerHeader const* const* layerPtrList,
	unsigned int layerCount) {
	return EndFrame_Hook_Impl(
		Real_ovr_SubmitFrame2,
		session,
		frameIndex,
		viewScaleDesc,
		layerPtrList,
		layerCount);
}

template <class T>
bool hook_libovr_function(const char* name, T** funcPtrPtr, T* hook) {
	if (!find_libovr_function(funcPtrPtr, name)) {
		return false;
	}
	auto err = DetourAttach((void**)funcPtrPtr, hook);
	if (!err) {
		dprint("- Hooked {} at {:#010x}", name, (intptr_t)*funcPtrPtr);
		return true;
	}

	dprint(" - Failed to hook {}: {}", name, err);
	return false;
}

#define HOOK_LIBOVR_FUNCTION(name) \
	hook_libovr_function(#name, &Real_##name, Hooked_##name)

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
	if (DetourIsHelperProcess()) {
		return TRUE;
	}

	if (dwReason == DLL_PROCESS_ATTACH) {
		dprint("Attached to process.");
		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateAllThreads();
#define IT(x) HOOK_LIBOVR_FUNCTION(x);
		HOOKED_OVR_FUNCS
#undef IT
#define IT(x) \
	Real_##x = reinterpret_cast<decltype(&x)>( \
		DetourFindFunction("LibOVRRT64_1.dll", #x));
		UNHOOKED_OVR_FUNCS
#undef IT
		auto err = DetourTransactionCommit();
		dprint("Installed hooks: {}", (int)err);
	} else if (dwReason == DLL_PROCESS_DETACH) {
		dprint("Detaching from process...");
		DetourTransactionBegin();
		DetourUpdateAllThreads();
#define IT(x) DetourDetach(&(PVOID&)Real_##x, Hooked_##x);
		HOOKED_FUNCS
#undef IT
		DetourTransactionCommit();
		dprint("Cleaned up Detours.");
	}
	return TRUE;
}