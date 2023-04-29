/*
    Ogler - Use OpenGL shaders in REAPER
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
*/

#include "ogler.hpp"

#include "ogler_editor.hpp"

#include <memory>
#include <span>
#include <thread>
#include <vector>
#include <winuser.h>

#define WIN32_LEAN_AND_MEAN
#include <commctrl.h>
#include <windows.h>

#include <Scintilla.h>

#include <ScintillaTypes.h>

#include <ScintillaCall.h>

namespace ogler {

ATOM cls_atom{};

static std::span<char> load_resource(const char *name, const char *type) {
  auto resinfo = FindResource(get_hinstance(), name, type);
  auto res = LoadResource(get_hinstance(), resinfo);
  auto size = SizeofResource(get_hinstance(), resinfo);

  return std::span<char>(reinterpret_cast<char *>(res), size);
}

OglerVst::Editor::Editor(void *parent, int &w, int &h, OglerVst &vst)
    : parent_wnd(static_cast<HWND>(parent)), width(w), height(h), vst(vst) {
  InitCommonControls();
  if (!cls_atom) {
    WNDCLASSEX cls{
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_DBLCLKS | CS_OWNDC,
        .lpfnWndProc = [](HWND hWnd, UINT Msg, WPARAM wParam,
                          LPARAM lParam) -> LRESULT {
          if (Msg == WM_CREATE) {
            auto strct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            auto editor = reinterpret_cast<Editor *>(strct->lpCreateParams);
            editor->child_wnd = hWnd;
            SetWindowLongPtr(hWnd, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(strct->lpCreateParams));
          }

          auto editor =
              reinterpret_cast<Editor *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

          switch (Msg) {
          case WM_CREATE: {
            editor->create();
            break;
          }
          case WM_SIZE:
            editor->resize();
            break;
          case WM_NOTIFY: {
            auto noti = reinterpret_cast<LPNMHDR>(lParam);
            if (noti->hwndFrom == editor->scintilla) {
              auto scnoti = reinterpret_cast<SCNotification *>(wParam);
              editor->scintilla_noti(noti->code, *scnoti);
              return 0;
            }
            break;
          }
          case WM_COMMAND: {
            if (HIWORD(wParam) == BN_CLICKED &&
                reinterpret_cast<HWND>(lParam) == editor->recompile_btn) {
              editor->recompile_clicked();
              return 0;
            }
          }
          default:
            return DefWindowProc(hWnd, Msg, wParam, lParam);
          }
          return 0;
        },
        .hInstance = get_hinstance(),
        .lpszClassName = "ogler_class"};
    cls_atom = RegisterClassEx(&cls);
  }

  CreateWindowEx(0, reinterpret_cast<LPCSTR>(cls_atom), "ogler",
                 WS_CHILD | WS_TABSTOP | WS_VISIBLE, 0, 0, width, height,
                 parent_wnd, nullptr, get_hinstance(), this);
}

OglerVst::Editor::~Editor() {
  vst.data.video_shader = sc_call->GetText(sc_call->TextLength());
  DestroyWindow(child_wnd);
}

void OglerVst::Editor::create() {
  recompile_btn =
      CreateWindowEx(0, "BUTTON", "Recompile",
                     WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 0,
                     0, 100, 50, child_wnd, nullptr, get_hinstance(), nullptr);

  scintilla = CreateWindowEx(
      0, "Scintilla", "",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN | SBS_SIZEGRIP, 0,
      50, 100, 100, child_wnd, nullptr, get_hinstance(), nullptr);

  auto fn_ = reinterpret_cast<Scintilla::FunctionDirect>(
      SendMessage(scintilla, SCI_GETDIRECTFUNCTION, 0, 0));
  auto ptr_ = SendMessage(scintilla, SCI_GETDIRECTPOINTER, 0, 0);
  sc_call = std::make_unique<Scintilla::ScintillaCall>();
  sc_call->SetFnPtr(fn_, ptr_);
  sc_call->SetText(vst.data.video_shader.c_str());

  sc_call->StyleSetFont(STYLE_DEFAULT, "Consolas");
  auto width = sc_call->TextWidth(STYLE_LINENUMBER, "_999");
  sc_call->SetMarginWidthN(0, width);

  statusbar = CreateWindowEx(0, STATUSCLASSNAME, "",
                             WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                             child_wnd, nullptr, get_hinstance(), nullptr);
}

void OglerVst::Editor::scintilla_noti(unsigned code,
                                      const SCNotification &noti) {}

void OglerVst::Editor::resize() {
  SendMessage(statusbar, WM_SIZE, 0, 0);
  RECT statusbar_size, size;
  GetClientRect(statusbar, &statusbar_size);
  GetClientRect(child_wnd, &size);
  SetWindowPos(recompile_btn, nullptr, 0, 0, size.right, 50, SWP_NOMOVE);
  SetWindowPos(scintilla, nullptr, 0, 50, size.right,
               size.bottom - statusbar_size.bottom - 50, SWP_NOMOVE);
}

void OglerVst::Editor::recompile_clicked() {
  vst.data.video_shader = sc_call->GetText(sc_call->TextLength());
  if (auto err = vst.recompile_shaders()) {
    MessageBox(child_wnd, err->c_str(), "Shader compilation error",
               MB_OK | MB_ICONERROR);
  }
}

void OglerVst::Editor::reload_source() {
  sc_call->SetText(vst.data.video_shader.c_str());
}

bool OglerVst::has_editor() noexcept { return true; }

void OglerVst::get_editor_bounds(std::int16_t &top, std::int16_t &left,
                                 std::int16_t &bottom,
                                 std::int16_t &right) noexcept {
  top = 0;
  left = 0;
  bottom = editor_h;
  right = editor_w;
}

void OglerVst::open_editor(void *hWnd) noexcept {
  editor = std::make_unique<Editor>(hWnd, editor_w, editor_h, *this);
}

void OglerVst::close_editor() noexcept { editor = nullptr; }

bool OglerVst::is_editor_open() noexcept { return editor != nullptr; }

} // namespace ogler