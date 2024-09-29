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
#include <future>
#endif

#include <bitset>

namespace
{
    std::bitset<LO_NUM_OPTIONS> g_Options;

	LayerResult LAYER_SetOptionBool(LayerOption option, bool value) {
		if (g_Options.size() > option) {
			g_Options[option] = value;
			return LR_SUCCESS;
		}
        return LR_INVALID_OPTION;
	}

#if defined (_WIN32)
    LayerResult LAYER_CC LAYER_ShouldCaptureFrame(const void* xrSession, uint64_t frame)
    {
		static std::future<int> mbfuture;
        static bool cancled = false;
        const bool blocking = g_Options[LO_BLOCKING_CAPTURE];

        if (!cancled && !mbfuture.valid()) {
			mbfuture = std::async(std::launch::async, [frame, xrSession, blocking]() ->int {
				char msg[64]{ '0' };
				if (blocking) {
					sprintf_s(msg, "Capture frame #%llu (Session %p) ?", frame, xrSession);
				}
				else {
					sprintf_s(msg, "Capture current frame (Session %p) ?", xrSession);
				}
				return MessageBoxA(NULL, msg, "OpenXR-RenderDoc-Layer", MB_YESNOCANCEL | MB_ICONQUESTION);
			});
        }

		if (mbfuture.valid() && (blocking || (mbfuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready))) {
            const int userInput = mbfuture.get();
            if (userInput == IDCANCEL) {
                cancled = true;
                return LR_SHOULD_SKIP_FRAME;
            } else if (userInput == IDNO) {
                return LR_SHOULD_SKIP_FRAME;
            }
            else if (userInput == IDYES) {
                return LR_SUCCESS;
            }
		}

        return LR_SHOULD_SKIP_FRAME;
    }

#else
    LayerResult LAYER_CC LAYER_ShouldCaptureFrame(const void* xrSession, uint64_t frame)
    {
        return LR_SUCCESS;
    }
#endif

} // !anon namespace

extern "C"
{
    // must match pLAYER_CONTROLL_GetAPI signature with name LAYER_CONTROLL_GetAPI
	LAYER_EXPORT LayerResult LAYER_CC LAYER_CONTROLL_GetAPI(LayerVersion version, void** outAPIPointers)
    {
        static RENDERDOC_OPENXR_LAYER_API_1_0_0 api{LAYER_ShouldCaptureFrame, LAYER_SetOptionBool};
        if(outAPIPointers != nullptr) {
            if(version == LayerVersion::LV_1_0_0) {
                *outAPIPointers = &api;
                return LayerResult::LR_SUCCESS;
            }
        }

        return LayerResult::LR_INVALID_API_VERSION;
    }
}