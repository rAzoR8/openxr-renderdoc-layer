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

#ifndef OPENXR_RENDERDOC_LAYER_H
#define OPENXR_RENDERDOC_LAYER_H 1

#include <stdint.h>
#include <stdbool.h>

// TODO: __cdecl for windows?
#ifndef LAYER_CC
#define LAYER_CC
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum LayerResult
{
    LR_SUCCESS = 0,
    LR_INVALID_API_VERSION = 1,
    LR_INVALID_OPTION = 2,
    LR_SHOULD_SKIP_FRAME = 3
};

enum LayerVersion
{
    LV_1_0_0 = 100
};

enum LayerOption {
    LO_BLOCKING_CAPTURE = 0,
    LO_NUM_OPTIONS
};

typedef LayerResult(LAYER_CC *pLAYER_ShouldCaptureFrame)(const void* xrSession, uint64_t frame);

typedef LayerResult(LAYER_CC *pLAYER_SetOptionBool)(LayerOption option, bool value);

typedef struct RENDERDOC_OPENXR_LAYER_API_1_0_0
{
    pLAYER_ShouldCaptureFrame ShouldCaptureFrame;
    pLAYER_SetOptionBool SetOptionBool;
} RENDERDOC_OPENXR_LAYER_API_1_0_0;

typedef LayerResult(LAYER_CC *pLAYER_CONTROLL_GetAPI)(LayerVersion version, void** outAPIPointers);

#ifndef LAYER_CONTROLL_FUNC
#define LAYER_CONTROLL_FUNC "LAYER_CONTROLL_GetAPI"
#endif

#ifdef __cplusplus
}    // extern "C"
#endif

#endif // !OPENXR_RENDERDOC_LAYER_H