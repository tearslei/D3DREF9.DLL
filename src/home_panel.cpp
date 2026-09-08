#include "home_panel.h"
#include "feature_manager.h"
#include "macro_engine.h"
#include "instant_sniper.h"
#include "aim_controller.h"
namespace d3dref9 {
namespace {
constexpr BYTE kPanelOpacity = 140; // 45% 透明 => 55% 不透明（Win32 alpha）
constexpr int kRowsY = 58;
constexpr int kRowHeight = 20;

bool IsYellowFeature(const std::wstring& label) {
    // 面板中显示的都是用户可切换项；自动启动项由 FeatureInfo::visible=false 隐藏。
    return label != L"自动瞄准+自动开枪";
}
HWND GameWindow(){return FindWindowW(L"CrossFire",nullptr);}
}
HomePanel& HomePanel::Instance(){static HomePanel p;return p;}
bool HomePanel::Create(HINSTANCE h){inst_=h;WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=h;wc.lpszClassName=L"d3dref9_autonomous_panel";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);hwnd_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_NOACTIVATE,L"d3dref9_autonomous_panel",L"d3dref9自治",WS_POPUP,8,8,320,420,GameWindow(),nullptr,h,this);if(!hwnd_)return false;SetLayeredWindowAttributes(hwnd_,0,kPanelOpacity,LWA_ALPHA);ShowWindow(hwnd_,SW_HIDE);SetTimer(hwnd_,1,100,nullptr);return true;}
void HomePanel::Destroy(){if(hwnd_){KillTimer(hwnd_,1);DestroyWindow(hwnd_);hwnd_=nullptr;}}
void HomePanel::Toggle(){if(!hwnd_)return;bool v=!visible_.load();visible_=v;if(v){if(auto owner=GameWindow())SetWindowLongPtrW(hwnd_,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(owner));SetWindowPos(hwnd_,HWND_TOPMOST,8,8,320,420,SWP_NOACTIVATE|SWP_SHOWWINDOW);}else ShowWindow(hwnd_,SW_HIDE);}
LRESULT CALLBACK HomePanel::WndProc(HWND w,UINT m,WPARAM wp,LPARAM lp){HomePanel*p=(HomePanel*)GetWindowLongPtrW(w,GWLP_USERDATA);if(m==WM_NCCREATE){p=(HomePanel*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)p);}return p?p->Handle(w,m,wp,lp):DefWindowProcW(w,m,wp,lp);}
LRESULT HomePanel::Handle(HWND w,UINT m,WPARAM wp,LPARAM lp){switch(m){case WM_MOUSEACTIVATE:return MA_NOACTIVATE;case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);Paint(dc);EndPaint(w,&ps);return 0;}case WM_TIMER:if(visible_.load())SetWindowPos(w,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);InvalidateRect(w,nullptr,FALSE);return 0;case WM_LBUTTONDOWN:{int row=GET_Y_LPARAM(lp);int n=(int)Features().PanelSnapshot().size();if(row>=20&&row<48)Features().ToggleMaster();else if(row>=kRowsY&&row<kRowsY+n*kRowHeight)Features().Toggle(Features().VisibleFeatureAt((size_t)((row-kRowsY)/kRowHeight)));else if(row>=kRowsY+(n+2)*kRowHeight&&row<kRowsY+(n+3)*kRowHeight)MacroEngine::Instance().Toggle();else if(row>=kRowsY+(n+6)*kRowHeight)MacroEngine::Instance().EmergencyStop();return 0;}case WM_CLOSE:visible_=false;ShowWindow(w,SW_HIDE);return 0;}return DefWindowProcW(w,m,wp,lp);}
void HomePanel::Paint(HDC dc){RECT r;GetClientRect(hwnd_,&r);HBRUSH bg=CreateSolidBrush(RGB(25,25,30));FillRect(dc,&r,bg);DeleteObject(bg);SetBkMode(dc,TRANSPARENT);HFONT f=CreateFontW(14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,0,0,L"Microsoft YaHei");HFONT old=(HFONT)SelectObject(dc,f);SetTextColor(dc,RGB(230,230,230));TextOutW(dc,12,6,L"d3dref9自治  Home 面板",22);SetTextColor(dc,Features().MasterEnabled()?RGB(120,255,140):RGB(255,120,100));std::wstring master=std::wstring(L"总开关  [")+(Features().MasterEnabled()?L"开":L"关")+L"]（点击切换）";TextOutW(dc,12,24,master.c_str(),(int)master.size());SetTextColor(dc,RGB(255,215,90));TextOutW(dc,12,42,L"黄色功能：点击行切换，也可使用对应热键",28);auto rows=Features().PanelSnapshot();int y=kRowsY;for(auto&x:rows){SetTextColor(dc,IsYellowFeature(x.first)?RGB(255,215,90):RGB(255,190,80));std::wstring s=x.first+L"  ["+(x.second?L"开":L"关")+L"]";TextOutW(dc,12,y,s.c_str(),(int)s.size());y+=kRowHeight;}std::wstring mode=L"开枪模式: "+std::wstring(InstantSniper::Instance().SniperMode()?L"瞬狙":L"普通");SetTextColor(dc,RGB(230,230,230));TextOutW(dc,12,y,mode.c_str(),(int)mode.size());y+=kRowHeight;if(!InstantSniper::Instance().SniperMode()){std::wstring bone=L"普通瞄准位置: "+std::wstring(AimController::Instance().NormalLabel())+L"（F10+6切换）";TextOutW(dc,12,y,bone.c_str(),(int)bone.size());y+=kRowHeight;}else{TextOutW(dc,12,y,L"瞬狙瞄准: 自动选择准心最近部位",28);y+=kRowHeight;}std::wstring m=std::wstring(L"键盘宏  [")+(MacroEngine::Instance().Enabled()?L"开":L"关")+L"]";TextOutW(dc,12,y,m.c_str(),(int)m.size());y+=kRowHeight;std::wstring st=std::wstring(L"宏状态: ")+(MacroEngine::Instance().Running()?L"运行中":L"空闲");TextOutW(dc,12,y,st.c_str(),(int)st.size());y+=kRowHeight;auto left=InstantSniper::Instance().CooldownLeft();std::wstring cool=left?L"瞬狙冷却中："+std::to_wstring(left)+L" ms":L"瞬狙冷却：就绪";SetTextColor(dc,left?RGB(255,190,80):RGB(180,220,180));TextOutW(dc,12,y,cool.c_str(),(int)cool.size());y+=kRowHeight;SetTextColor(dc,RGB(255,120,100));TextOutW(dc,12,y,L"宏急停（点击立即停止）",22);SelectObject(dc,old);DeleteObject(f);}
}
