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

#include "ogler_editor.hpp"

#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ogler {
class EditorScripting : public sciter::om::asset<EditorScripting> {
  EditorInterface &plugin;

public:
  EditorScripting(EditorInterface &plugin) : plugin(plugin) {}

  void recompile() { plugin.recompile_shaders(); }

  void set_parameter(int index, float value) {
    plugin.set_parameter(index, value);
  }

  const std::string &get_shader_source() { return plugin.get_shader_source(); }
  bool set_shader_source(const std::string &source) {
    plugin.set_shader_source(source);
    return true;
  }

  int get_zoom() { return plugin.get_zoom(); }
  bool set_zoom(int value) {
    plugin.set_zoom(value);
    return true;
  }

  int get_editor_width() { return plugin.get_width(); }
  bool set_editor_width(int width) {
    plugin.set_width(width);
    return true;
  }

  int get_editor_height() { return plugin.get_height(); }
  bool set_editor_height(int height) {
    plugin.set_height(height);
    return true;
  }

  SOM_PASSPORT_BEGIN_EX(ogler, EditorScripting)
  SOM_FUNCS(SOM_FUNC(recompile), SOM_FUNC(set_parameter), )
  SOM_PROPS(SOM_VIRTUAL_PROP(shader_source, get_shader_source,
                             set_shader_source),
            SOM_VIRTUAL_PROP(zoom, get_zoom, set_zoom),
            SOM_VIRTUAL_PROP(editor_width, get_editor_width, set_editor_width),
            SOM_VIRTUAL_PROP(editor_height, get_editor_height,
                             set_editor_height), )
  SOM_PASSPORT_END
};

Editor::Editor(HWND hWnd, HINSTANCE hinstance, HMENU hMenu, HWND hwndParent,
               int cy, int cx, int y, int x, LONG style, const char *name,
               const char *cls, DWORD exStyle,
               std::unique_ptr<EditorInterface> plugin)
    : hwnd(hWnd), plugin(std::move(plugin)) {}

void Editor::window_created() {
  auto value = sciter::value::wrap_asset(new EditorScripting(*this->plugin));
  SciterSetVariable(hwnd, "ogler", &value);
  SciterLoadFile(hwnd, L"res://ui/index.html");
}

Editor::~Editor() {
  sciter::value val = {};
  SciterSetVariable(hwnd, "ogler", &val);
}

void Editor::resize(int w, int h) {
  plugin->set_width(w);
  plugin->set_height(h);
}

void Editor::reload_source() {
  BEHAVIOR_EVENT_PARAMS evt{
      .cmd = CUSTOM,
      .name = L"shader_reload",
  };
  BOOL handled;
  SciterFireEvent(&evt, true, &handled);
}

void Editor::compiler_error(const std::string &error) {
  auto data = sciter::value::make_map({
      {"compiler_error", ""},
  });
  BEHAVIOR_EVENT_PARAMS evt{
      .cmd = CUSTOM,
      .data = data,
      .name = L"compiler_error",
  };
  BOOL handled;
  SciterFireEvent(&evt, true, &handled);
}

void Editor::params_changed(const std::vector<Parameter> &params) {
  std::vector<sciter::value> ret;
  for (const auto &param : params) {
    ret.push_back(sciter::value::make_map({
        {"value", param.value},
        {"name", param.info.name},
        {"displayName", param.info.display_name},
        {"defaultValue", param.info.default_value},
        {"minimumVal", param.info.minimum_val},
        {"maximumVal", param.info.maximum_val},
        {"middleValue", param.info.middle_value},
        {"stepSize", param.info.step_size},
    }));
  }
  auto data = sciter::value::make_map({
      {"parameters", sciter::value::make_array(ret.size(), ret.data())},
  });
  BEHAVIOR_EVENT_PARAMS evt{
      .cmd = CUSTOM,
      .data = data,
      .name = L"params_changed",
  };
  BOOL handled;
  SciterFireEvent(&evt, true, &handled);
}

} // namespace ogler