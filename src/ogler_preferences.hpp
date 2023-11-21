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

#include <string>

#include <sciter-js/sciter-x.h>

#include "sciter_window.hpp"

namespace ogler {
class Preferences : public sciter::om::asset<Preferences> {
  const char *file;

public:
  Preferences(const char *file);

  std::string get_font_face() const;
  bool set_font_face(const std::string &name);

  int get_font_size() const;
  bool set_font_size(int size);

  int get_view_ws() const;
  bool set_view_ws(int value);

  bool get_use_tabs() const;
  bool set_use_tabs(bool value);

  int get_tab_width() const;
  bool set_tab_width(int value);

  SOM_PASSPORT_BEGIN_EX(ogler, Preferences)
  SOM_FUNCS()
  SOM_PROPS(SOM_VIRTUAL_PROP(font_face, get_font_face, set_font_face),
            SOM_VIRTUAL_PROP(font_size, get_font_size, set_font_size),
            SOM_VIRTUAL_PROP(view_ws, get_view_ws, set_view_ws),
            SOM_VIRTUAL_PROP(use_tabs, get_use_tabs, set_use_tabs),
            SOM_VIRTUAL_PROP(tab_width, get_tab_width, set_tab_width), )
  SOM_PASSPORT_END
};

class PreferencesWindow final : public SciterWindow<PreferencesWindow> {
  friend SciterWindow<PreferencesWindow>;

  HWND hwnd;
  const char *file;

protected:
  void window_created() override;

public:
  static constexpr const char *class_name = "ogler_preferences";
  PreferencesWindow(HWND hWnd, HINSTANCE hinstance, HMENU hMenu,
                    HWND hwndParent, int cy, int cx, int y, int x, LONG style,
                    const char *name, const char *cls, DWORD exStyle,
                    const char *file);
  virtual ~PreferencesWindow();
};
} // namespace ogler