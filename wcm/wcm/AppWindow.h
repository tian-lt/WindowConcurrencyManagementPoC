#pragma once

// winrt
#include <winrt/base.h>
#include <winrt/windows.foundation.h>

// windows
#include <Windows.h>

namespace {

constexpr auto WindowClassName = TEXT("WCMWindowClass");

} // namespace

class AppWindow {
private:
  AppWindow() = default;

public:
  static constexpr WORD UserCommandBase = 1000;

  void Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  static AppWindow Create(HINSTANCE hinst, int cmdshow) {
    AppWindow wnd;
    static auto clss = RegisterWindowClass(hinst);
    winrt::check_bool(clss);
    wnd.hwnd_ = CreateWindowEx(
        0, WindowClassName, TEXT("Demo"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, 800, 800, nullptr, nullptr, hinst, &wnd);
    winrt::check_bool(wnd.hwnd_);
    wnd.CreateBuiltinControls();
    UpdateWindow(wnd.hwnd_);
    ShowWindow(wnd.hwnd_, cmdshow);
    return wnd;
  }

  void UpdateCounter(int value) {
    SetWindowTextW(txtCounter_, std::to_wstring(value).c_str());
  }

  void UpdateText(const std::wstring &text) {
    SetWindowTextW(txtCounter_, text.c_str());
  }

  template <class F> void CreateCommand(WORD cmd, LPCTSTR displayName, F &&f) {
    winrt::check_bool(CreateWindow(
        TEXT("BUTTON"), displayName,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10,
        usrCtrlOffset_, 200, 30, hwnd_, (HMENU)cmd,
        (HINSTANCE)GetWindowLongPtr(hwnd_, GWLP_HINSTANCE), nullptr));
    cmds_[cmd] = [f = std::forward<F>(f)](AppWindow *wnd) { f(wnd); };
    usrCtrlOffset_ += 50;
  }

  HWND Handle() const noexcept { return hwnd_; }

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam) {
    switch (msg) {
    case WM_COMMAND:
      if (DispatchCommand(hwnd, LOWORD(wparam))) {
        return 0;
      }
      break;
    case WM_CREATE: {
      auto *creation = reinterpret_cast<CREATESTRUCT *>(lparam);
      SetWindowLongPtr(hwnd, GWLP_USERDATA,
                       reinterpret_cast<LONG_PTR>(creation->lpCreateParams));
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
  }

  static ATOM RegisterWindowClass(HINSTANCE hinst) {
    WNDCLASSEX wcex = {.cbSize = sizeof(WNDCLASSEX),
                       .style = CS_SAVEBITS | CS_DROPSHADOW | CS_HREDRAW |
                                CS_VREDRAW,
                       .lpfnWndProc = WndProc,
                       .hInstance = hinst,
                       .hCursor = LoadCursor(nullptr, IDC_ARROW),
                       .hbrBackground = (HBRUSH)COLOR_WINDOW,
                       .lpszClassName = WindowClassName};
    return RegisterClassEx(&wcex);
  }

  static bool DispatchCommand(HWND hwnd, WORD cmd) {
    auto wnd = GetThis(hwnd);
    auto entry = wnd->cmds_.find(cmd);
    if (entry != wnd->cmds_.cend()) {
      entry->second(wnd);
      return true;
    }
    return false;
  }

  static AppWindow *GetThis(HWND hwnd) {
    return reinterpret_cast<AppWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  void CreateBuiltinControls() {
    txtCounter_ = CreateWindow(
        TEXT("STATIC"), TEXT("0"), WS_VISIBLE | WS_CHILD | SS_LEFT, 10, 10, 160,
        30, hwnd_, nullptr, (HINSTANCE)GetWindowLongPtr(hwnd_, GWLP_HINSTANCE),
        nullptr);
    winrt::check_bool(txtCounter_);
  }

private:
  std::unordered_map<WORD, std::function<void(AppWindow *)>> cmds_;
  HWND hwnd_ = nullptr;
  HWND txtCounter_ = nullptr;
  int usrCtrlOffset_ = 50;
};
