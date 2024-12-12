// std
#include <chrono>
#include <functional>
#include <thread>
#include <unordered_map>

// wil
#include <wil/coroutine.h>

// app headers
#include "AppWindow.h"

#pragma comment(linker, "/manifestdependency:\"type='win32' "                  \
                        "name='Microsoft.Windows.Common-Controls' "            \
                        "version='6.0.0.0' "                                   \
                        "processorArchitecture='*' "                           \
                        "publicKeyToken='6595b64144ccf1df' "                   \
                        "language='*' "                                        \
                        "\"")
#pragma comment(lib, "WindowsApp.lib")

namespace {

enum struct DemoCommand : WORD {
  None = AppWindow::UserCommandBase,
  Countdown5s,
  CancelWhenInBkgrd,
};

}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int cmdshow) {
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  std::jthread windowThread([hinst, cmdshow] {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto wnd = AppWindow::Create(hinst, cmdshow);
    wnd.CreateCommand(std::to_underlying(DemoCommand::Countdown5s),
                      TEXT("5s Countdown"),
                      [](AppWindow *wnd) -> winrt::fire_and_forget {
                        winrt::apartment_context ctx;
                        for (int i = 0; i < 5; ++i) {
                          wnd->UpdateCounter(5 - i);
                          co_await winrt::resume_after(std::chrono::seconds{1});
                          co_await ctx;
                        }
                        wnd->UpdateText(L"Done");
                      });
    wnd.CreateCommand(
        std::to_underlying(DemoCommand::CancelWhenInBkgrd),
        TEXT("Cancel when in background"),
        [](AppWindow *wnd) -> winrt::fire_and_forget {
          auto task = [&]() -> winrt::Windows::Foundation::IAsyncAction {
            auto cancelToken = co_await winrt::get_cancellation_token();
            cancelToken.enable_propagation();
            winrt::apartment_context apartment;
            co_await winrt::resume_after(std::chrono::seconds{10});
            co_await apartment;
            wnd->UpdateText(
                L"Failed to cancel. Hope you don't see this message.");
          }();
          [](AppWindow *wnd, auto *task) -> winrt::fire_and_forget {
            wnd->UpdateText(L"Cancel in 1s ...");
            co_await winrt::resume_after(std::chrono::seconds{1});
            task->Cancel();
          }(wnd, &task);
          try {
            co_await task;
          } catch (const winrt::hresult_canceled &) {
            wnd->UpdateText(L"Cancelled");
          }
        });
    wnd.Run();
    winrt::uninit_apartment();
  });
  windowThread.join();
  std::this_thread::sleep_for(std::chrono::seconds{
      2}); // wait for a while to observe potential race condition problems.
  winrt::uninit_apartment();
  return 0;
}
