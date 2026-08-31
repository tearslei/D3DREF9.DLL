#pragma once
#include "common.h"
namespace d3dref9 { void Initialize(HINSTANCE); void Shutdown(); }
extern "C" __declspec(dllexport) void d3dref9_initialize();
extern "C" __declspec(dllexport) void d3dref9_shutdown();
