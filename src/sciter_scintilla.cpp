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

#include "sciter_scintilla.hpp"

#include <sciter-js/sciter-x.h>

#include <Scintilla.h>
#include <ScintillaTypes.h>

#include <ScintillaCall.h>

#include "ogler_lexer.hpp"
#include "sciter_window.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ogler {
class ScintillaEditor final : public sciter::event_handler {
public:
  HWND wnd{}, scintilla{};
  HINSTANCE hinstance{};
  HELEMENT self{}; // note: weak ref (not addrefed)
  std::unique_ptr<Scintilla::ScintillaCall> sc_call;

  ScintillaEditor(HINSTANCE hinstance) : hinstance(hinstance) {}

  virtual void attached(HELEMENT he) override {
    static ClassHandle cls_atom = [this]() -> ClassHandle {
      WNDCLASSEX cls{
          .cbSize = sizeof(WNDCLASSEX),
          .lpfnWndProc = [](HWND hWnd, UINT Msg, WPARAM wParam,
                            LPARAM lParam) -> LRESULT {
            if (Msg == WM_CREATE) {
              auto strct = reinterpret_cast<LPCREATESTRUCT>(lParam);
              auto window =
                  reinterpret_cast<ScintillaEditor *>(strct->lpCreateParams);
              SetWindowLongPtr(
                  hWnd, GWLP_USERDATA,
                  reinterpret_cast<LONG_PTR>(strct->lpCreateParams));
              window->wnd = hWnd;
            }
            BOOL bHandled;

            auto lResult = SciterProcND(hWnd, Msg, wParam, lParam, &bHandled);
            if (bHandled) {
              return lResult;
            }

            auto window = reinterpret_cast<ScintillaEditor *>(
                GetWindowLongPtr(hWnd, GWLP_USERDATA));

            switch (Msg) {
            case WM_CREATE:
              window->scintilla =
                  CreateWindow(TEXT("Scintilla"), TEXT(""),
                               WS_CHILD | WS_VSCROLL | WS_HSCROLL |
                                   WS_CLIPCHILDREN | WS_VISIBLE | WS_EX_LAYERED,
                               0, 0, 0, 0, hWnd, 0, window->hinstance, 0);
            case WM_SIZE:
              SetWindowPos(window->scintilla, nullptr, 0, 0, LOWORD(lParam),
                           HIWORD(lParam), 0);
              break;
            case WM_NOTIFY: {
              NMHDR *hdr = reinterpret_cast<NMHDR *>(lParam);
              if (hdr->hwndFrom == window->scintilla) {
                SCNotification *noti = reinterpret_cast<SCNotification *>(hdr);
                auto data = sciter::value::make_map();
                BEHAVIOR_EVENT_PARAMS evt{
                    .cmd = CUSTOM,
                    .heTarget = window->self,
                    .he = window->self,
                };
                switch (hdr->code) {
#define EVT_FIELD(NAME) data.set_item(#NAME, noti->NAME)
#define EVT_FIELD_LL(NAME) data.set_item(#NAME, static_cast<double>(noti->NAME))
                case SCN_STYLENEEDED:
                  evt.name = L"style_needed";
                  EVT_FIELD_LL(position);
                  break;
                case SCN_CHARADDED:
                  evt.name = L"char_added";
                  EVT_FIELD(ch);
                  EVT_FIELD(characterSource);
                  break;
                case SCN_SAVEPOINTREACHED:
                  evt.name = L"save_point_reached";
                  break;
                case SCN_SAVEPOINTLEFT:
                  evt.name = L"save_point_left";
                  break;
                case SCN_MODIFYATTEMPTRO:
                  evt.name = L"modify_attempt_ro";
                  break;
                case SCN_KEY:
                  evt.name = L"key";
                  EVT_FIELD(ch);
                  EVT_FIELD(modifiers);
                  break;
                case SCN_DOUBLECLICK:
                  evt.name = L"double_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  EVT_FIELD_LL(line);
                  break;
                case SCN_UPDATEUI:
                  evt.name = L"update_ui";
                  EVT_FIELD(updated);
                  break;
                case SCN_MODIFIED:
                  evt.name = L"modified";
                  EVT_FIELD_LL(position);
                  EVT_FIELD(modificationType);
                  EVT_FIELD(text);
                  EVT_FIELD_LL(length);
                  EVT_FIELD_LL(linesAdded);
                  EVT_FIELD_LL(line);
                  EVT_FIELD(foldLevelNow);
                  EVT_FIELD(foldLevelPrev);
                  EVT_FIELD(token);
                  EVT_FIELD_LL(annotationLinesAdded);
                  break;
                case SCN_MACRORECORD:
                  evt.name = L"macro_record";
                  EVT_FIELD(message);
                  EVT_FIELD_LL(wParam);
                  EVT_FIELD_LL(lParam);
                  break;
                case SCN_MARGINCLICK:
                  evt.name = L"margin_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  EVT_FIELD(margin);
                  break;
                case SCN_NEEDSHOWN:
                  evt.name = L"need_shown";
                  EVT_FIELD_LL(position);
                  EVT_FIELD_LL(length);
                  break;
                case SCN_PAINTED:
                  evt.name = L"painted";
                  break;
                case SCN_USERLISTSELECTION:
                  evt.name = L"user_list_selection";
                  EVT_FIELD(listType);
                  EVT_FIELD(text);
                  EVT_FIELD_LL(position);
                  EVT_FIELD(ch);
                  EVT_FIELD(listCompletionMethod);
                  break;
                case SCN_URIDROPPED:
                  evt.name = L"uri_dropped";
                  EVT_FIELD(text);
                  break;
                case SCN_DWELLSTART:
                  evt.name = L"dwell_start";
                  EVT_FIELD_LL(position);
                  EVT_FIELD(x);
                  EVT_FIELD(y);
                  break;
                case SCN_DWELLEND:
                  evt.name = L"dwell_end";
                  EVT_FIELD_LL(position);
                  EVT_FIELD(x);
                  EVT_FIELD(y);
                  break;
                case SCN_ZOOM:
                  evt.name = L"zoom";
                  break;
                case SCN_HOTSPOTCLICK:
                  evt.name = L"hot_spot_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  break;
                case SCN_HOTSPOTDOUBLECLICK:
                  evt.name = L"hot_spot_double_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  break;
                case SCN_CALLTIPCLICK:
                  evt.name = L"call_tip_click";
                  EVT_FIELD_LL(position);
                  break;
                case SCN_AUTOCSELECTION:
                  evt.name = L"auto_c_selection";
                  EVT_FIELD(text);
                  EVT_FIELD_LL(position);
                  EVT_FIELD(ch);
                  EVT_FIELD(listCompletionMethod);
                  break;
                case SCN_INDICATORCLICK:
                  evt.name = L"indicator_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  break;
                case SCN_INDICATORRELEASE:
                  evt.name = L"indicator_release";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  break;
                case SCN_AUTOCCANCELLED:
                  evt.name = L"auto_c_cancelled";
                  break;
                case SCN_AUTOCCHARDELETED:
                  evt.name = L"auto_c_char_deleted";
                  break;
                case SCN_HOTSPOTRELEASECLICK:
                  evt.name = L"hot_spot_release_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  break;
                case SCN_FOCUSIN:
                  evt.name = L"focus_in";
                  break;
                case SCN_FOCUSOUT:
                  evt.name = L"focus_out";
                  break;
                case SCN_AUTOCCOMPLETED:
                  evt.name = L"auto_c_completed";
                  EVT_FIELD(text);
                  EVT_FIELD_LL(position);
                  EVT_FIELD(ch);
                  EVT_FIELD(listCompletionMethod);
                  break;
                case SCN_MARGINRIGHTCLICK:
                  evt.name = L"margin_right_click";
                  EVT_FIELD(modifiers);
                  EVT_FIELD_LL(position);
                  EVT_FIELD(margin);
                  break;
                case SCN_AUTOCSELECTIONCHANGE:
                  evt.name = L"auto_c_selection_change";
                  EVT_FIELD(listType);
                  EVT_FIELD(text);
                  EVT_FIELD_LL(position);
                  break;
                }
                evt.data = data;
                BOOL handled;
                // SciterSendEvent(window->self, CUSTOM, nullptr, 0, &handled);
                SciterFireEvent(&evt, false, &handled);
                break;
              }
            }
            default:
              return DefWindowProc(hWnd, Msg, wParam, lParam);
            }
            return 0;
          },
          .hInstance = hinstance,
          .lpszClassName = TEXT("sciter_scintilla"),
      };
      return {RegisterClassEx(&cls), hinstance};
    }();
    self = he;
    sciter::dom::element el = he;
    auto hwnd = CreateWindow(cls_atom, TEXT(""),
                             WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE, 0, 0, 0,
                             0, el.get_element_hwnd(true), 0, hinstance, this);
    assert(hwnd);
    el.attach_hwnd(wnd);

    auto fn_ = reinterpret_cast<Scintilla::FunctionDirect>(
        SendMessage(scintilla, SCI_GETDIRECTFUNCTION, 0, 0));
    auto ptr_ = SendMessage(scintilla, SCI_GETDIRECTPOINTER, 0, 0);
    sc_call = std::make_unique<Scintilla::ScintillaCall>();
    sc_call->SetFnPtr(fn_, ptr_);

    sc_call->SetILexer(new GlslLexer());
  }

  virtual void detached(HELEMENT he) override {
    if (wnd && IsWindow(wnd)) {
      sc_call = nullptr;
      DestroyWindow(wnd);
    }
    wnd = nullptr;
    self = nullptr;
    sciter::dom::element el = he;
    el.attach_hwnd(nullptr);
  }

  int rgb(int r, int g, int b) { return r | (g << 8) | (b << 16); }

  int rgba(int r, int g, int b, int a) {
    return r | (g << 8) | (b << 16) | (a << 24);
  }

  std::string get_text() { return sc_call->GetText(sc_call->TextLength()); }
  bool set_text(const std::string &text) {
    sc_call->SetText(text.c_str());
    return true;
  }

  int get_tab_width() { return sc_call->TabWidth(); }
  bool set_tab_width(int value) {
    sc_call->SetTabWidth(value);
    return true;
  }

  bool get_use_tabs() { return sc_call->UseTabs(); }
  bool set_use_tabs(bool value) {
    sc_call->SetUseTabs(value);
    return true;
  }

  void style_set_font(int style, const std::string &font) {
    sc_call->StyleSetFont(style, font.c_str());
  }

  void style_set_fore(int style, int fore) {
    sc_call->StyleSetFore(style, fore);
  }

  void style_set_back(int style, int back) {
    sc_call->StyleSetBack(style, back);
  }

  void style_set_size(int style, int size) {
    sc_call->StyleSetSize(style, size);
  }

  void set_element_color(int element, int color) {
    sc_call->SetElementColour(static_cast<Scintilla::Element>(element), color);
  }

  int text_width(int style, const std::string &text) {
    return sc_call->TextWidth(style, text.c_str());
  }

  void set_margin_width(int margin, int width) {
    sc_call->SetMarginWidthN(margin, width);
  }

  int get_zoom() { return sc_call->Zoom(); }

  bool set_zoom(int value) {
    sc_call->SetZoom(value);
    return true;
  }

  void annotation_clear_all() { sc_call->AnnotationClearAll(); }

  void annotation_set_text(int line, const std::string &text) {
    sc_call->AnnotationSetText(line, text.c_str());
  }

  void annotation_set_style(int line, int style) {
    sc_call->AnnotationSetStyle(line, style);
  }

  int annotation_get_visible() {
    return static_cast<int>(sc_call->AnnotationGetVisible());
  }

  bool annotation_set_visible(int type) {
    sc_call->AnnotationSetVisible(
        static_cast<Scintilla::AnnotationVisible>(type));
    return true;
  }

  void empty_undo_buffer() { sc_call->EmptyUndoBuffer(); }

  bool set_viewws(int value) {
    sc_call->SetViewWS(static_cast<Scintilla::WhiteSpace>(value));
    return true;
  }
  int get_viewws() { return static_cast<int>(sc_call->ViewWS()); }

  bool set_readonly(bool value) {
    sc_call->SetReadOnly(value);
    return true;
  }
  bool get_readonly() { return sc_call->ReadOnly(); }

  SOM_PASSPORT_BEGIN(ScintillaEditor)
  SOM_FUNCS(SOM_FUNC(rgb), SOM_FUNC(rgba), SOM_FUNC(style_set_font),
            SOM_FUNC(style_set_fore), SOM_FUNC(style_set_back),
            SOM_FUNC(style_set_size), SOM_FUNC(set_element_color),
            SOM_FUNC(text_width), SOM_FUNC(set_margin_width),
            SOM_FUNC(annotation_clear_all), SOM_FUNC(annotation_set_text),
            SOM_FUNC(annotation_set_style), SOM_FUNC(empty_undo_buffer))
  SOM_PROPS(SOM_VIRTUAL_PROP(text, get_text, set_text),
            SOM_VIRTUAL_PROP(tab_width, get_tab_width, set_tab_width),
            SOM_VIRTUAL_PROP(use_tabs, get_use_tabs, set_use_tabs),
            SOM_VIRTUAL_PROP(zoom, get_zoom, set_zoom),
            SOM_VIRTUAL_PROP(annotation_visible, annotation_get_visible,
                             annotation_set_visible),
            SOM_VIRTUAL_PROP(view_ws, get_viewws, set_viewws),
            SOM_VIRTUAL_PROP(readonly, get_readonly, set_readonly))
  SOM_PASSPORT_END
};

ScintillaEditorFactory::ScintillaEditorFactory(HINSTANCE hinstance)
    : behavior_factory("scintilla"), hinstance(hinstance) {}

sciter::event_handler *ScintillaEditorFactory::create(HELEMENT he) {
  return new ScintillaEditor(hinstance);
}
} // namespace ogler