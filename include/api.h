#pragma once
#include "common.h"
namespace d3dref9 { bool Initialize(HINSTANCE); void Shutdown(); }
#if defined(D3DREF9_DIRECT)
// The replacement binary must expose only the reference ordinal-1 export.
// Keep these helpers link-visible internally, but do not add named exports.
#define D3DREF9_API
#else
#define D3DREF9_API __declspec(dllexport)
#endif
extern "C" D3DREF9_API void d3dref9_initialize();
extern "C" D3DREF9_API void d3dref9_shutdown();
