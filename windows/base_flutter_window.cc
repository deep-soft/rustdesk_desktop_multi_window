//
// Created by yangbin on 2022/1/27.
//

#include "base_flutter_window.h"

#include <dwmapi.h>
#include <iostream>
// #include <shobjidl_core.h>

#pragma comment(lib, "dwmapi.lib")

namespace {
void CenterRectToMonitor(LPRECT prc) {
  HMONITOR hMonitor;
  MONITORINFO mi;
  RECT rc;
  int w = prc->right - prc->left;
  int h = prc->bottom - prc->top;

  //
  // get the nearest monitor to the passed rect.
  //
  hMonitor = MonitorFromRect(prc, MONITOR_DEFAULTTONEAREST);

  //
  // get the work area or entire monitor rect.
  //
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hMonitor, &mi);

  rc = mi.rcMonitor;

  prc->left = rc.left + (rc.right - rc.left - w) / 2;
  prc->top = rc.top + (rc.bottom - rc.top - h) / 2;
  prc->right = prc->left + w;
  prc->bottom = prc->top + h;

}

std::wstring Utf16FromUtf8(const std::string &string) {
  int size_needed = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), -1, nullptr, 0);
  if (size_needed == 0) {
    return {};
  }
  std::wstring wstrTo(size_needed, 0);
  int converted_length = MultiByteToWideChar(CP_UTF8, 0, string.c_str(), -1, &wstrTo[0], size_needed);
  if (converted_length == 0) {
    return {};
  }
  return wstrTo;
}

}

void BaseFlutterWindow::Center() {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  RECT rc;
  GetWindowRect(handle, &rc);
  CenterRectToMonitor(&rc);
  SetWindowPos(handle, nullptr, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void BaseFlutterWindow::Focus() {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  Restore();
  SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(handle);
}

void BaseFlutterWindow::SetFullscreen(bool fullscreen) {
    auto window = GetWindowHandle();
    if (!window) {
        return;
    }

    // Inspired by how Chromium does this
    // https://src.chromium.org/viewvc/chrome/trunk/src/ui/views/win/fullscreen_handler.cc?revision=247204&view=markup

    // Save current window state if not already fullscreen.
    if (!g_is_window_fullscreen) {
        // Save current window information.
        g_maximized_before_fullscreen = !!::IsZoomed(window);
        g_style_before_fullscreen = GetWindowLong(window, GWL_STYLE);
        g_ex_style_before_fullscreen = GetWindowLong(window, GWL_EXSTYLE);
        if (g_maximized_before_fullscreen) {
            SendMessage(window, WM_SYSCOMMAND, SC_RESTORE, 0);
        }
        ::GetWindowRect(window, &g_frame_before_fullscreen);
        g_title_bar_style_before_fullscreen = title_bar_style_;
        g_is_frameless_before_fullscreen = is_frameless_;
    }

    if (fullscreen) {
        flutter::EncodableMap args2 = flutter::EncodableMap();
        args2[flutter::EncodableValue("titleBarStyle")] =
            flutter::EncodableValue("normal");
        SetTitleBarStyle(args2);

        // Set new window style and size.
        ::SetWindowLong(window, GWL_STYLE,
            g_style_before_fullscreen & ~(WS_CAPTION | WS_THICKFRAME));
        ::SetWindowLong(window, GWL_EXSTYLE,
            g_ex_style_before_fullscreen &
            ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
                WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

        MONITORINFO monitor_info;
        monitor_info.cbSize = sizeof(monitor_info);
        ::GetMonitorInfo(::MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
            &monitor_info);
        ::SetWindowPos(window, NULL, monitor_info.rcMonitor.left,
            monitor_info.rcMonitor.top,
            monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
            monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        ::SendMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }
    else {
        ::SetWindowLong(window, GWL_STYLE, g_style_before_fullscreen);
        ::SetWindowLong(window, GWL_EXSTYLE, g_ex_style_before_fullscreen);

        SendMessage(window, WM_SYSCOMMAND, SC_RESTORE, 0);

        if (title_bar_style_ != g_title_bar_style_before_fullscreen) {
            flutter::EncodableMap args2 = flutter::EncodableMap();
            args2[flutter::EncodableValue("titleBarStyle")] =
                flutter::EncodableValue(g_title_bar_style_before_fullscreen);
            SetTitleBarStyle(args2);
        }

        if (g_is_frameless_before_fullscreen)
            SetAsFrameless();

        if (g_maximized_before_fullscreen) {
            flutter::EncodableMap args2 = flutter::EncodableMap();
            args2[flutter::EncodableValue("vertically")] =
                flutter::EncodableValue(false);
            Maximize(args2);
        }
        else {
            ::SetWindowPos(
                window, NULL, g_frame_before_fullscreen.left,
                g_frame_before_fullscreen.top,
                g_frame_before_fullscreen.right - g_frame_before_fullscreen.left,
                g_frame_before_fullscreen.bottom - g_frame_before_fullscreen.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
    g_is_window_fullscreen = fullscreen;
}

void BaseFlutterWindow::StartDragging() {
    auto window = GetWindowHandle();
    if (!window) {
        return;
    }
    ReleaseCapture();
    SendMessage(window, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

bool BaseFlutterWindow::IsMaximized() { 
    auto window = GetWindowHandle();
    if (!window) {
        return false;
    }
    WINDOWPLACEMENT windowPlacement;
    GetWindowPlacement(window, &windowPlacement);

    return windowPlacement.showCmd == SW_MAXIMIZE;
}

void BaseFlutterWindow::Maximize() {
    auto window = GetWindowHandle();
    if (!window) {
        return;
    }
    WINDOWPLACEMENT windowPlacement;
    GetWindowPlacement(window, &windowPlacement);
    // non vertical now
    if (windowPlacement.showCmd != SW_MAXIMIZE) {
        PostMessage(window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }
}

void BaseFlutterWindow::Unmaximize() {
    auto window = GetWindowHandle();
    if (!window) {
        return;
    }
    WINDOWPLACEMENT windowPlacement;
    GetWindowPlacement(window, &windowPlacement);

    if (windowPlacement.showCmd != SW_NORMAL) {
        PostMessage(window, WM_SYSCOMMAND, SC_RESTORE, 0);
    }
}

void BaseFlutterWindow::Minimize() {
    auto window = GetWindowHandle();
    if (!window) {
        return;
    }
    WINDOWPLACEMENT windowPlacement;
    GetWindowPlacement(window, &windowPlacement);

    if (windowPlacement.showCmd != SW_SHOWMINIMIZED) {
        PostMessage(window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    }
}

void BaseFlutterWindow::ShowTitlebar(bool show) {
    auto window = GetWindowHandle();
    if (!window) {
        return;
    }
    this->title_bar_style_ = show ? "normal" : "hidden";
    if (!show) {
        LONG lStyle = GetWindowLong(window, GWL_STYLE);
        SetWindowLong(window, GWL_STYLE, lStyle & ~WS_CAPTION);
        SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOSIZE
            | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    else {
        LONG lStyle = GetWindowLong(window, GWL_STYLE);
        SetWindowLong(window, GWL_STYLE, lStyle | WS_CAPTION);
        SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOSIZE
            | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

void BaseFlutterWindow::Maximize(const flutter::EncodableMap& args) {
    bool vertically =
        std::get<bool>(args.at(flutter::EncodableValue("vertically")));

    HWND hwnd = GetWindowHandle();
    WINDOWPLACEMENT windowPlacement;
    GetWindowPlacement(hwnd, &windowPlacement);

    if (vertically) {
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        PostMessage(hwnd, WM_NCLBUTTONDBLCLK, HTTOP,
            MAKELPARAM(cursorPos.x, cursorPos.y));
    }
    else {
        if (windowPlacement.showCmd != SW_MAXIMIZE) {
            PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }
    }
}

void BaseFlutterWindow::SetTitleBarStyle(const flutter::EncodableMap& args) {
    title_bar_style_ =
        std::get<std::string>(args.at(flutter::EncodableValue("titleBarStyle")));
    // Enables the ability to go from setAsFrameless() to
    // TitleBarStyle.normal/hidden
    is_frameless_ = false;

    MARGINS margins = { 0, 0, 0, 0 };
    HWND hWnd = GetWindowHandle();
    RECT rect;
    GetWindowRect(hWnd, &rect);
    DwmExtendFrameIntoClientArea(hWnd, &margins);
    SetWindowPos(hWnd, nullptr, rect.left, rect.top, 0, 0,
        SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE |
        SWP_FRAMECHANGED);
    std::cout << "set title bar styled" << std::endl;
}

void BaseFlutterWindow::SetAsFrameless() {
    is_frameless_ = true;
    HWND hWnd = GetWindowHandle();

    RECT rect;

    GetWindowRect(hWnd, &rect);
    SetWindowPos(hWnd, nullptr, rect.left, rect.top, rect.right - rect.left,
        rect.bottom - rect.top,
        SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE |
        SWP_FRAMECHANGED);
}


void BaseFlutterWindow::Restore() {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(handle, &windowPlacement);

  if (windowPlacement.showCmd != SW_NORMAL) {
    PostMessage(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
  }
}

void BaseFlutterWindow::SetBounds(double_t x, double_t y, double_t width, double_t height) {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  MoveWindow(handle, int32_t(x), int32_t(y),
             static_cast<int>(width),
             static_cast<int>(height),
             TRUE);
}

void BaseFlutterWindow::SetTitle(const std::string &title) {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  SetWindowText(handle, Utf16FromUtf8(title).c_str());
}

void BaseFlutterWindow::Close() {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  PostMessage(handle, WM_SYSCOMMAND, SC_CLOSE, 0);
}

void BaseFlutterWindow::Show() {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  ShowWindowAsync(handle, SW_SHOW);
  SetForegroundWindow(handle);
}

void BaseFlutterWindow::Hide() {
  auto handle = GetWindowHandle();
  if (!handle) {
    return;
  }
  ShowWindow(handle, SW_HIDE);
}
