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

#pragma once

#include "ogler.hpp"

struct SCNotification;
namespace Scintilla {
class ScintillaCall;
}

namespace ogler {
struct OglerVst::Editor {
  HWND parent_wnd, child_wnd, scintilla, statusbar, recompile_btn;
  std::unique_ptr<Scintilla::ScintillaCall> sc_call;

  int &width;
  int &height;

  OglerVst &vst;

  Editor(void *hWnd, int &w, int &h, OglerVst &vst);
  ~Editor();

  void reload_source();

private:
  void create();
  void resize();
  void recompile_clicked();
  void scintilla_noti(unsigned code, const SCNotification &);
};
} // namespace ogler