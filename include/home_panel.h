#pragma once
#include "common.h"
#include <windowsx.h>
namespace d3dref9 {
class HomePanel {
public: static HomePanel& Instance(); bool Create(HINSTANCE); void Destroy(); void Toggle(); bool Visible()const{return visible_.load();} HWND Hwnd()const{return hwnd_;}
private: HomePanel()=default; static LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM); LRESULT Handle(HWND,UINT,WPARAM,LPARAM); void Paint(HDC); HWND hwnd_{}; HINSTANCE inst_{}; std::atomic<bool> visible_{false};
};
}
