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

#include "compile_shader.hpp"
#include "sciter_window.hpp"

namespace ogler {
class EditorInterface {
public:
  virtual ~EditorInterface() = default;

  virtual void recompile_shaders() = 0;
  virtual void set_shader_source(const std::string &source) = 0;
  virtual const std::string &get_shader_source() = 0;
  virtual int get_zoom() = 0;
  virtual void set_zoom(int zoom) = 0;
  virtual int get_width() = 0;
  virtual int get_height() = 0;
  virtual void set_width(int w) = 0;
  virtual void set_height(int h) = 0;

  virtual void set_parameter(size_t index, float value) = 0;

  virtual const char *get_ini_file() = 0;
};

class Editor final : public SciterWindow<Editor> {
  friend SciterWindow<Editor>;
  HWND hwnd;
  std::unique_ptr<EditorInterface> plugin;

protected:
  void window_created() override;

public:
  static constexpr WinStr class_name = TEXT("ogler");
  Editor(HWND hWnd, HINSTANCE hinstance, HMENU hMenu, HWND hwndParent, int cy,
         int cx, int y, int x, LONG style, WinStr name, WinStr cls,
         DWORD exStyle, std::unique_ptr<EditorInterface> plugin);
  virtual ~Editor();

  void reload_source();
  void resize(int w, int h) final;
  void compiler_error(const std::string &error);
  void params_changed(const std::vector<Parameter> &params);
};
} // namespace ogler