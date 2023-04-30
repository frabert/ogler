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
#include <regex>
#include <span>
#include <sstream>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <commctrl.h>
#include <windows.h>

#include <Scintilla.h>

#include <ScintillaTypes.h>

#include <ScintillaCall.h>

#include "ogler_lexer.hpp"
#include "ogler_styles.hpp"

namespace ogler {

static constexpr int rgb(int r, int g, int b) {
  return r | (g << 8) | (b << 16);
}

static constexpr int rgba(int r, int g, int b, int a) {
  return r | (g << 8) | (b << 16) | (a << 24);
}

ATOM cls_atom{};

OglerVst::Editor::Editor(void *parent, int &w, int &h, OglerVst &vst)
    : parent_wnd(static_cast<HWND>(parent)), width(w), height(h), vst(vst) {
  InitCommonControls();
  if (!cls_atom) {
    WNDCLASSEX cls{
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_DBLCLKS,
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
            editor->resize(LOWORD(lParam), HIWORD(lParam));
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

  scintilla = CreateWindowEx(0, "Scintilla", "",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL,
                             0, 50, 100, 100, child_wnd, nullptr,
                             get_hinstance(), nullptr);

  auto fn_ = reinterpret_cast<Scintilla::FunctionDirect>(
      SendMessage(scintilla, SCI_GETDIRECTFUNCTION, 0, 0));
  auto ptr_ = SendMessage(scintilla, SCI_GETDIRECTPOINTER, 0, 0);
  sc_call = std::make_unique<Scintilla::ScintillaCall>();
  sc_call->SetFnPtr(fn_, ptr_);
  reload_source();

  sc_call->SetViewWS(Scintilla::WhiteSpace::VisibleAlways);
  sc_call->SetTabWidth(4);
  sc_call->SetUseTabs(false);

  sc_call->StyleSetFont(STYLE_DEFAULT, "Consolas");
  auto width = sc_call->TextWidth(STYLE_LINENUMBER, "_999");
  sc_call->SetMarginWidthN(0, width);

  sc_call->StyleSetFore(STY_Keyword, rgb(0x00, 0x00, 0xFF));
  sc_call->StyleSetFore(STY_Type, rgb(0x00, 0x80, 0x80));
  sc_call->StyleSetFore(STY_Integer, rgb(0x4B, 0x00, 0x82));
  sc_call->StyleSetFore(STY_Float, rgb(0x4B, 0x00, 0x82));
  sc_call->StyleSetFore(STY_Bool, rgb(0x00, 0x0, 0xFF));
  sc_call->StyleSetFore(STY_String, rgb(0x80, 0x00, 0x00));
  sc_call->StyleSetFore(STY_BuiltinFunc, rgb(0x8b, 0x00, 0x8b));
  sc_call->StyleSetFore(STY_Punctuation, 0);
  sc_call->StyleSetFore(STY_Comment, rgb(0x00, 0x64, 0x00));

  sc_call->StyleSetBack(STY_ErrorAnnotation, 0xFFCCCCFF);

  sc_call->SetILexer(new GlslLexer());
}

void OglerVst::Editor::scintilla_noti(unsigned code,
                                      const SCNotification &noti) {}

void OglerVst::Editor::resize(int w, int h) {
  SetWindowPos(recompile_btn, nullptr, 0, 0, w, 50, SWP_NOMOVE);
  SetWindowPos(scintilla, nullptr, 0, 50, w, h - 50, SWP_NOMOVE);
  width = w;
  height = h;
}

void OglerVst::Editor::recompile_clicked() {
  sc_call->AnnotationClearAll();
  vst.data.video_shader = sc_call->GetText(sc_call->TextLength());
  if (auto err = vst.recompile_shaders()) {
    auto err_str = std::move(*err);
    std::istringstream err_stream(err_str);
    auto err_regex = std::regex(R"(ERROR: <source>:(\d+): (.+))");
    std::string line;
    while (std::getline(err_stream, line)) {
      std::smatch match;
      if (std::regex_match(line, match, err_regex)) {
        auto line = std::stoi(match[1].str());
        auto msg = match[2].str();

        sc_call->AnnotationSetText(line - 1, msg.c_str());
        sc_call->AnnotationSetStyle(line - 1, STY_ErrorAnnotation);
        sc_call->AnnotationSetVisible(Scintilla::AnnotationVisible::Indented);
      }
    }

    MessageBox(child_wnd, err_str.c_str(), "Shader compilation error",
               MB_OK | MB_ICONERROR);
  }
}

void OglerVst::Editor::reload_source() {
  sc_call->SetText(vst.data.video_shader.c_str());
  sc_call->EmptyUndoBuffer();
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

void OglerVst::editor_idle() noexcept {
  if (!editor) {
    return;
  }

  auto parent_parent = GetParent(editor->parent_wnd);
  RECT parent_rect, parent_parent_rect;
  GetWindowRect(parent_parent, &parent_parent_rect);
  GetWindowRect(editor->parent_wnd, &parent_rect);
  auto w = parent_parent_rect.right - parent_parent_rect.left;
  auto h = parent_parent_rect.bottom - parent_rect.top;
  SetWindowPos(editor->parent_wnd, nullptr, 0, 0, w, h, SWP_NOMOVE);
  SetWindowPos(editor->child_wnd, nullptr, 0, 0, w, h, SWP_NOMOVE);
}

bool OglerVst::is_editor_open() noexcept { return editor != nullptr; }

} // namespace ogler