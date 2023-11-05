/*
    Ogler - Use GLSL shaders in REAPER
    Copyright (C) 2023  Francesco Bertolaccini <francesco@bertolaccini.dev>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    Additional permission under GNU GPL version 3 section 7

    If you modify this Program, or any covered work, by linking or
    combining it with Sciter (or a modified version of that library),
    containing parts covered by the terms of Sciter's EULA, the licensors
    of this Program grant you additional permission to convey the
    resulting work.
*/

#pragma once

#include <sciter-js/sciter-x.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string_view>

#include "ogler_resources.hpp"

namespace ogler {

template <typename Derived> class SciterWindow {
public:
  HWND hwnd;
  HINSTANCE hinstance;

  virtual void resize(int width, int height) {}

protected:
  virtual SC_LOAD_DATA_RETURN_CODES sciter_load_data(LPSCN_LOAD_DATA pnmld) {
    std::wstring_view uri(pnmld->uri);

    if (!uri.starts_with(WSTR("this://app/"))) {
      return LOAD_OK;
    }

    auto adata = get_resource(uri.substr(11));
    if (adata.empty()) {
      return LOAD_OK;
    }

    SciterDataReady(pnmld->hwnd, pnmld->uri, adata.data(), adata.size());
    return LOAD_OK;
  }
  virtual void sciter_data_loaded(LPSCN_DATA_LOADED) {}
  virtual bool sciter_attach_behavior(LPSCN_ATTACH_BEHAVIOR lpab) {
    sciter::event_handler *pb =
        sciter::behavior_factory::create(lpab->behaviorName, lpab->element);
    if (pb) {
      lpab->elementTag = pb;
      lpab->elementProc = sciter::event_handler::element_proc;
      return true;
    }
    return false;
  }
  virtual void sciter_engine_destroyed(LPSCN_ENGINE_DESTROYED) {}
  virtual void sciter_posted_notification(LPSCN_POSTED_NOTIFICATION) {}
  virtual void
  sciter_graphics_critical_failure(LPSCN_GRAPHICS_CRITICAL_FAILURE) {}
  virtual void sciter_keyboard_request(LPSCN_KEYBOARD_REQUEST) {}
  virtual void sciter_invalidate_rect(LPSCN_INVALIDATE_RECT) {}
  virtual void sciter_set_cursor(LPSCN_SET_CURSOR) {}

  virtual ~SciterWindow() { DestroyWindow(hwnd); }
  SciterWindow(HWND parent, HINSTANCE hinstance, int width, int height,
               std::string_view title)
      : hinstance(hinstance) {
    static ATOM cls_atom = 0;
    if (!cls_atom) {
      WNDCLASSEX cls{
          .cbSize = sizeof(WNDCLASSEX),
          .style = CS_HREDRAW | CS_VREDRAW,
          .lpfnWndProc = [](HWND hWnd, UINT Msg, WPARAM wParam,
                            LPARAM lParam) -> LRESULT {
            if (Msg == WM_CREATE) {
              auto strct = reinterpret_cast<LPCREATESTRUCT>(lParam);
              auto window =
                  reinterpret_cast<SciterWindow *>(strct->lpCreateParams);
              window->hwnd = hWnd;
              SetWindowLongPtr(
                  hWnd, GWLP_USERDATA,
                  reinterpret_cast<LONG_PTR>(strct->lpCreateParams));
            }
            BOOL bHandled;

            auto lResult = SciterProcND(hWnd, Msg, wParam, lParam, &bHandled);
            if (bHandled) {
              return lResult;
            }

            auto window = reinterpret_cast<SciterWindow *>(
                GetWindowLongPtr(hWnd, GWLP_USERDATA));

            switch (Msg) {
            case WM_CREATE: {
#ifndef NDEBUG
              SciterSetOption(hWnd, SCITER_SET_DEBUG_MODE, TRUE);
#endif
              SciterSetCallback(
                  hWnd,
                  [](LPSCITER_CALLBACK_NOTIFICATION pns,
                     LPVOID callbackParam) -> UINT {
                    auto window =
                        reinterpret_cast<SciterWindow *>(callbackParam);
                    switch (pns->code) {
                    case SC_LOAD_DATA:
                      return static_cast<UINT>(window->sciter_load_data(
                          reinterpret_cast<LPSCN_LOAD_DATA>(pns)));
                    case SC_DATA_LOADED:
                      window->sciter_data_loaded(
                          reinterpret_cast<LPSCN_DATA_LOADED>(pns));
                      return 0;
                    case SC_ATTACH_BEHAVIOR:
                      return static_cast<UINT>(window->sciter_attach_behavior(
                          reinterpret_cast<LPSCN_ATTACH_BEHAVIOR>(pns)));
                    case SC_ENGINE_DESTROYED:
                      window->sciter_engine_destroyed(
                          reinterpret_cast<LPSCN_ENGINE_DESTROYED>(pns));
                      return 0;
                    case SC_POSTED_NOTIFICATION:
                      window->sciter_posted_notification(
                          reinterpret_cast<LPSCN_POSTED_NOTIFICATION>(pns));
                      return 0;
                    case SC_GRAPHICS_CRITICAL_FAILURE:
                      window->sciter_graphics_critical_failure(
                          reinterpret_cast<LPSCN_GRAPHICS_CRITICAL_FAILURE>(
                              pns));
                      return 0;
                    case SC_KEYBOARD_REQUEST:
                      window->sciter_keyboard_request(
                          reinterpret_cast<LPSCN_KEYBOARD_REQUEST>(pns));
                      return 0;
                    case SC_INVALIDATE_RECT:
                      window->sciter_invalidate_rect(
                          reinterpret_cast<LPSCN_INVALIDATE_RECT>(pns));
                      return 0;
                    case SC_SET_CURSOR:
                      window->sciter_set_cursor(
                          reinterpret_cast<LPSCN_SET_CURSOR>(pns));
                      return 0;
                    default:
                      assert(0);
                      return 0;
                    }
                  },
                  window);
              break;
            }
            case WM_SIZE:
              window->resize(LOWORD(lParam), HIWORD(lParam));
              break;
            default:
              return DefWindowProc(hWnd, Msg, wParam, lParam);
            }
            return 0;
          },
          .hInstance = hinstance,
          .lpszClassName = Derived::class_name,
      };
      cls_atom = RegisterClassEx(&cls);
    }

    CreateWindowEx(0, reinterpret_cast<LPCSTR>(cls_atom), title.data(),
                   (parent ? WS_CHILD : 0) | WS_VISIBLE | WS_TABSTOP |
                       WS_CLIPCHILDREN,
                   0, 0, width, height, parent, nullptr, hinstance, this);
    assert(hwnd);
  }
};
} // namespace ogler