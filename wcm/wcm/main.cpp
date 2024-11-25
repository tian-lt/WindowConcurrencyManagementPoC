// std
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>

// winrt
#include <winrt/base.h>

// wil
#include <wil/coroutine.h>

// windows
#include <Windows.h>

#pragma comment(linker, "/manifestdependency:\"type='win32' "                  \
                        "name='Microsoft.Windows.Common-Controls' "            \
                        "version='6.0.0.0' "                                   \
                        "processorArchitecture='*' "                           \
                        "publicKeyToken='6595b64144ccf1df' "                   \
                        "language='*' "                                        \
                        "\"")
#pragma comment(lib, "WindowsApp.lib")

namespace {

constexpr auto WindowClassName = TEXT("WCMWindowClass");
enum struct DemoCommand : WORD {
  None = 1000,
  BadTask,
};

class Window {
private:
  Window() = default;

public:
  void Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  static Window Create(HINSTANCE hinst, int cmdshow) {
    Window wnd;
    static auto clss = RegisterWindowClass(hinst);
    winrt::check_bool(clss);
    wnd.hwnd_ = CreateWindowEx(
        0, WindowClassName, TEXT("Demo"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, 800, 800, nullptr, nullptr, hinst, &wnd);
    winrt::check_bool(wnd.hwnd_);
    wnd.CreateControls();
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
    auto entry = wnd->cmds_.find(static_cast<DemoCommand>(cmd));
    if (entry != wnd->cmds_.cend()) {
      entry->second(wnd);
      return true;
    }
    return false;
  }

  static Window *GetThis(HWND hwnd) {
    return reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  template <class F>
  void CreateCommand(int &offset, DemoCommand cmd, LPCTSTR displayName, F &&f) {
    winrt::check_bool(CreateWindow(
        TEXT("BUTTON"), displayName,
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, offset, 200,
        30, hwnd_, (HMENU)cmd,
        (HINSTANCE)GetWindowLongPtr(hwnd_, GWLP_HINSTANCE), nullptr));
    cmds_[DemoCommand::BadTask] = [f = std::forward<F>(f)](Window *wnd) {
      f(wnd);
    };
    offset += 50;
  }

  void CreateControls() {
    txtCounter_ = CreateWindow(
        TEXT("STATIC"), TEXT("0"), WS_VISIBLE | WS_CHILD | SS_LEFT, 10, 10, 160,
        30, hwnd_, nullptr, (HINSTANCE)GetWindowLongPtr(hwnd_, GWLP_HINSTANCE),
        nullptr);
    winrt::check_bool(txtCounter_);
    int cmdOffset = 50;
    CreateCommand(cmdOffset, DemoCommand::BadTask, TEXT("Bad Task (5s)"),
                  [](Window *wnd) -> winrt::fire_and_forget {
                    winrt::apartment_context ctx;
                    for (int i = 0; i < 5; ++i) {
                      wnd->UpdateCounter(5 - i);
                      co_await winrt::resume_after(std::chrono::seconds{1});
                      co_await ctx;
                    }
                    wnd->UpdateText(L"Done");
                  });
  }

private:
  std::unordered_map<DemoCommand, std::function<void(Window *)>> cmds_;
  HWND hwnd_ = nullptr;
  HWND txtCounter_ = nullptr;
};

} // namespace

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int cmdshow) {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  std::jthread windowThread([hinst, cmdshow] {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto wnd = Window::Create(hinst, cmdshow);
    wnd.Run();
    winrt::uninit_apartment();
  });
  windowThread.join();
  std::this_thread::sleep_for(std::chrono::seconds{
      5}); // wait for a while to observe potential race condition problems.
  winrt::uninit_apartment();
  return 0;
}
