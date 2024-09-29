// MIT License

// Copyright (c) 2024 Fabian Wahlster

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Layer.h"

#define XR_NO_PROTOTYPES 1

#include "openxr.h"
#include "openxr_loader_negotiation.h"
#include "renderdoc_app.h"

#include <unordered_map>
#include <cstring>
#include <cstdio>

#ifndef __has_declspec_attribute
#define __has_declspec_attribute(x) 0
#endif

#if __has_declspec_attribute(dllexport) || defined(_MSC_VER)
#define LAYER_EXPORT __declspec(dllexport)
#else
#define LAYER_EXPORT __attribute__((dllexport))
#endif

#if defined (_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
	PFN_xrGetInstanceProcAddr s_xrGetInstanceProcAddr{ nullptr };
	PFN_xrCreateSession s_xrCreateSession{ nullptr };
	PFN_xrDestroySession s_xrDestroySession{ nullptr };
	PFN_xrBeginFrame s_xrBeginFrame{ nullptr };
	PFN_xrEndFrame s_xrEndFrame{ nullptr };

	RENDERDOC_API_1_0_0* s_renderDoc{ nullptr };
	RENDERDOC_OPENXR_LAYER_API_1_0_0* s_layerControl{ nullptr };

	struct SessionInfo {
		RENDERDOC_DevicePointer device{};
		uint64_t frame{ 0 };

		SessionInfo(RENDERDOC_DevicePointer _device, uint64_t _frame) : device(_device), frame(_frame) {}
	};

	std::unordered_map<XrSession, SessionInfo> s_sessionDevices;

	struct XrStructHeader {
		XrStructureType type;
		XrStructHeader* next;
	};

	struct BindingD3D11 {
		XrStructureType             type;
		const void* XR_MAY_ALIAS    next;
		void*						device; // ID3D11Device
	};

	struct BindingD3D12 {
		XrStructureType             type;
		const void* XR_MAY_ALIAS    next;
		void*						device;	// ID3D12Device
		void*						queue;	// ID3D12CommandQueue
	};

	struct BindingVulkan {
		XrStructureType             type;
		const void* XR_MAY_ALIAS    next;
		void*						instance;		// VkInstance
		void*						physicalDevice; // VkPhysicalDevice
		void*						device;			// VkDevice
		uint32_t                    queueFamilyIndex;
		uint32_t                    queueIndex;
	};

	struct BindingOpenGLWin32 {
		XrStructureType             type;
		const void* XR_MAY_ALIAS    next;
		void*						hDC;	// HDC
		void*						hGLRC;	// HGLRC
	};

	template<typename FuncPtr>
	FuncPtr LoadFunc(const char* libName, const char* libPath, const char* entryPoint) {
#ifdef _WIN32
		HMODULE mod = GetModuleHandleA(libName);
		if (mod == NULL) mod = LoadLibraryA(libPath);
		if (mod == NULL) mod = LoadLibraryA(libName);
		if (mod)
			return (FuncPtr)GetProcAddress(mod, entryPoint);
#else
		void* mod = dlopen(libName, RTLD_NOW | RTLD_NOLOAD);
		if (!mod) mod = dlopen(libPath, RTLD_NOW);
		if (!mod) mod = dlopen(libName, RTLD_NOW);
		if (mod)
			return (FuncPtr)dlsym(mod, entryPoint);
#endif
		return nullptr;
	}

	pRENDERDOC_GetAPI GetRenderDocApi()
	{
#ifdef _WIN32
		constexpr const char* renderdocLib = "renderdoc.dll";
#else
		constexpr const char* renderdocLib = "librenderdoc.so";
#endif
		// we don't have a fullpath for renderdoc lib
		return LoadFunc<pRENDERDOC_GetAPI>(renderdocLib, renderdocLib, "RENDERDOC_GetAPI");
	}

	pLAYER_CONTROLL_GetAPI GetLayerApi()
	{
		constexpr const char* controlLibName = LAYER_CONTROL_LIB_NAME;
		constexpr const char* controlLibPath = LAYER_CONTROL_LIB_PATH;
		constexpr const char* conrolEntryPoint = LAYER_CONTROLL_FUNC;
		return LoadFunc<pLAYER_CONTROLL_GetAPI>(controlLibName, controlLibPath, conrolEntryPoint);
	}

	XRAPI_ATTR XrResult XRAPI_CALL layer_xrCreateSession(
		XrInstance instance,
		const XrSessionCreateInfo* createInfo,
		XrSession* session)
	{
		if (s_xrCreateSession == nullptr)
			return XR_ERROR_RUNTIME_FAILURE;

		auto res = s_xrCreateSession(instance, createInfo, session);
		if (res == XR_SUCCESS && session != nullptr && createInfo->next != nullptr) {
			RENDERDOC_DevicePointer device{ nullptr };

			const XrStructHeader* next = reinterpret_cast<const XrStructHeader*>(createInfo->next);
			if (next->type == XR_TYPE_GRAPHICS_BINDING_D3D11_KHR) {
				device = reinterpret_cast<const BindingD3D11*>(next)->device;
			}
			else if (next->type == XR_TYPE_GRAPHICS_BINDING_D3D12_KHR) {
				device = reinterpret_cast<const BindingD3D12*>(next)->device;
			}
			else if (next->type == XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR) {
				device = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(reinterpret_cast<const BindingVulkan*>(next)->instance);
			}
			else if (next->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR) {
				device = reinterpret_cast<const BindingOpenGLWin32*>(next)->hGLRC;
			}

			s_sessionDevices.emplace(*session, SessionInfo{ device, 0 });
		}
		return res;
	}

	XRAPI_ATTR XrResult XRAPI_CALL layer_xrDestroySession(XrSession session)
	{
		s_sessionDevices.erase(session);

		if (s_xrDestroySession == nullptr)
			return XR_ERROR_RUNTIME_FAILURE;

		return s_xrDestroySession(session);
	}

	XRAPI_ATTR XrResult XRAPI_CALL layer_xrBeginFrame(
		XrSession session,
		const XrFrameBeginInfo* frameBeginInfo)
	{
		if (s_xrBeginFrame == nullptr)
			return XR_ERROR_RUNTIME_FAILURE;

		if (s_renderDoc != nullptr) {
			auto it = s_sessionDevices.find(session);
			if (it != s_sessionDevices.end()) {
				if (s_layerControl == nullptr || s_layerControl->ShouldCaptureFrame(session, it->second.frame) == LR_SUCCESS) {
					s_renderDoc->StartFrameCapture(it->second.device, nullptr);
				}
				it->second.frame++;
			}
		}

		return s_xrBeginFrame(session, frameBeginInfo);
	}

	XRAPI_ATTR XrResult XRAPI_CALL layer_xrEndFrame(
		XrSession session,
		const XrFrameEndInfo* frameEndInfo)
	{
		if (s_xrEndFrame == nullptr)
			return XR_ERROR_RUNTIME_FAILURE;

		if (s_renderDoc != nullptr) {
			auto it = s_sessionDevices.find(session);
			if (s_renderDoc->IsFrameCapturing() && it != s_sessionDevices.end()) {
				s_renderDoc->EndFrameCapture(it->second.device, nullptr);
				if (!s_renderDoc->IsTargetControlConnected()) {
					s_renderDoc->LaunchReplayUI(true, "");
				}
			}
		}

		return s_xrEndFrame(session, frameEndInfo);
	}

	// install our hooks
	XRAPI_ATTR XrResult XRAPI_CALL layer_xrGetInstanceProcAddr(
		XrInstance instance,
		const char* name,
		PFN_xrVoidFunction* function)
	{
		if (s_xrGetInstanceProcAddr == nullptr)
			return XR_ERROR_HANDLE_INVALID;

		if (strcmp(name, "xrCreateSession") == 0) {
			*function = (PFN_xrVoidFunction)layer_xrCreateSession;
			return XR_SUCCESS;
		}
		else if (strcmp(name, "xrDestroySession") == 0) {
			*function = (PFN_xrVoidFunction)layer_xrDestroySession;
			return XR_SUCCESS;
		}
		else if (strcmp(name, "xrBeginFrame") == 0) {
			*function = (PFN_xrVoidFunction)layer_xrBeginFrame;
			return XR_SUCCESS;
		}
		else if (strcmp(name, "xrEndFrame") == 0) {
			*function = (PFN_xrVoidFunction)layer_xrEndFrame;
			return XR_SUCCESS;
		}

		return s_xrGetInstanceProcAddr(instance, name, function);
	}

	XRAPI_ATTR XrResult XRAPI_CALL layer_xrCreateApiLayerInstance(
		const XrInstanceCreateInfo* info,
		const XrApiLayerCreateInfo* layerInfo,
		XrInstance* instance)
	{
		printf("xrCreateApiLayerInstance: installing RenderDoc hooks...\n");

		s_xrGetInstanceProcAddr = layerInfo->nextInfo->nextGetInstanceProcAddr;

		XrApiLayerCreateInfo nextApiLayerInfo = *layerInfo;
		nextApiLayerInfo.nextInfo = layerInfo->nextInfo->next;
		auto res = layerInfo->nextInfo->nextCreateApiLayerInstance(info, &nextApiLayerInfo, instance);

		if (res == XR_SUCCESS)
		{
			s_xrGetInstanceProcAddr(*instance, "xrCreateSession", (PFN_xrVoidFunction*)&s_xrCreateSession);
			s_xrGetInstanceProcAddr(*instance, "xrDestroySession", (PFN_xrVoidFunction*)&s_xrDestroySession);
			s_xrGetInstanceProcAddr(*instance, "xrBeginFrame", (PFN_xrVoidFunction*)&s_xrBeginFrame);
			s_xrGetInstanceProcAddr(*instance, "xrEndFrame", (PFN_xrVoidFunction*)&s_xrEndFrame);

			if (s_renderDoc == nullptr) {
				pRENDERDOC_GetAPI getApi = GetRenderDocApi();
				if (getApi != nullptr) {
					const int ret = getApi(eRENDERDOC_API_Version_1_0_0, (void**)&s_renderDoc);
					if (s_renderDoc != nullptr && ret == 1) {
						RENDERDOC_InputButton keys[2] = { eRENDERDOC_Key_F12, eRENDERDOC_Key_PrtScrn };
						s_renderDoc->SetCaptureKeys(keys, 2);
						s_renderDoc->SetCaptureTitle("OpenXR Layer Capture");
						printf("xrCreateApiLayerInstance: RenderDoc hook sucessfull. Keys: F12, PrtScrn\n");
					}
				}
			}

			if (s_layerControl == nullptr) {
				pLAYER_CONTROLL_GetAPI getApi = GetLayerApi();
				if (getApi != nullptr) {
					if (LR_SUCCESS == getApi(LayerVersion::LV_1_0_0, (void**)&s_layerControl)) {
						printf("xrCreateApiLayerInstance: Layer-Control available\n");
						// Override options:
						// s_layerControl->SetOptionBool(LO_BLOCKING_CAPTURE, false);
					}
				}
			}
		}

		return res;
	}
}

extern "C"
{
	XRAPI_ATTR LAYER_EXPORT XrResult XRAPI_CALL xrNegotiateLoaderApiLayerInterface(
		const XrNegotiateLoaderInfo* loaderInfo,
		const char* layerName,
		XrNegotiateApiLayerRequest* apiLayerRequest)
	{
#if defined(_DEBUG) && defined(_WIN32)
		while (!IsDebuggerPresent()) {
			printf("RenderDoc layer waiting for debugger. Press X to continue...\n");

			char input{};
			(void)scanf_s("  %c", &input, 1);
			if (input == 'x' || input == 'X') break;
		}
#endif

		printf("xrNegotiateLoaderApiLayerInterface: RenderDoc Layer https://github.com/rAzoR8/openxr-renderdoc-layer\n");

		if (loaderInfo == nullptr || apiLayerRequest == nullptr) {
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		if (loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO || apiLayerRequest->structType != XR_LOADER_INTERFACE_STRUCT_API_LAYER_REQUEST) {
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		if (loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) || apiLayerRequest->structSize != sizeof(XrNegotiateApiLayerRequest)) {
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		if (XR_CURRENT_LOADER_API_LAYER_VERSION < loaderInfo->minInterfaceVersion || XR_CURRENT_LOADER_API_LAYER_VERSION > loaderInfo->maxInterfaceVersion) {
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		if (XR_CURRENT_API_VERSION < loaderInfo->minApiVersion || XR_CURRENT_API_VERSION > loaderInfo->maxApiVersion) {
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		if (layerName != nullptr && strcmp(layerName, "XR_RENDERDOC_LAYER") != 0) {
			return XR_ERROR_INITIALIZATION_FAILED;
		}

		apiLayerRequest->layerInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
		apiLayerRequest->layerApiVersion = XR_CURRENT_API_VERSION;
		apiLayerRequest->getInstanceProcAddr = layer_xrGetInstanceProcAddr;
		apiLayerRequest->createApiLayerInstance = layer_xrCreateApiLayerInstance;

		return XR_SUCCESS;
	}
}