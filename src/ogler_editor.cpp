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

#include "ogler.hpp"

#include "ogler_editor.hpp"

#include <memory>
#include <regex>
#include <span>
#include <sstream>
#include <thread>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ogler_lexer.hpp"
#include "ogler_styles.hpp"

namespace ogler {
class EditorScripting : public sciter::om::asset<EditorScripting> {
  Ogler &plugin;

public:
  EditorScripting(Ogler &plugin) : plugin(plugin) {}

  sciter::value recompile() {
    if (auto err = plugin.recompile_shaders()) {
      return *err;
    }
    return {};
  }

  const std::string &get_shader_source() { return plugin.data.video_shader; }
  bool set_shader_source(const std::string &source) {
    plugin.data.video_shader = source;
    return true;
  }

  int get_zoom() { return plugin.data.editor_zoom; }
  bool set_zoom(int value) {
    plugin.data.editor_zoom = value;
    return true;
  }

  int get_editor_width() { return plugin.data.editor_w; }
  bool set_editor_width(int width) {
    plugin.data.editor_w = width;
    return true;
  }

  int get_editor_height() { return plugin.data.editor_h; }
  bool set_editor_height(int height) {
    plugin.data.editor_h = height;
    return true;
  }

  SOM_PASSPORT_BEGIN_EX(ogler, EditorScripting)
  SOM_FUNCS(SOM_FUNC(recompile), )
  SOM_PROPS(SOM_VIRTUAL_PROP(shader_source, get_shader_source,
                             set_shader_source),
            SOM_VIRTUAL_PROP(zoom, get_zoom, set_zoom),
            SOM_VIRTUAL_PROP(editor_width, get_editor_width, set_editor_width),
            SOM_VIRTUAL_PROP(editor_height, get_editor_height,
                             set_editor_height))
  SOM_PASSPORT_END
};

Ogler::Editor::Editor(HWND parent, Ogler &plugin)
    : SciterWindow(parent, plugin.data.editor_w, plugin.data.editor_h, "ogler"),
      plugin(plugin) {
  auto value = sciter::value::wrap_asset(new EditorScripting(plugin));
  SciterSetVariable(hwnd, "ogler", &value);
  SciterLoadFile(hwnd, L"this://app/index.html");
}

void Ogler::Editor::resize(int w, int h) {
  plugin.data.editor_w = w;
  plugin.data.editor_h = h;
}

void Ogler::Editor::reload_source() {
  BEHAVIOR_EVENT_PARAMS evt{
      .cmd = CUSTOM,
      .name = L"shader_reload",
  };
  BOOL handled;
  SciterFireEvent(&evt, false, &handled);
}

bool Ogler::gui_is_api_supported(std::string_view api, bool is_floating) {
  return api == CLAP_WINDOW_API_WIN32;
}

std::optional<std::pair<const char *, bool>> Ogler::gui_get_preferred_api() {
  return {{CLAP_WINDOW_API_WIN32, false}};
}

bool Ogler::gui_create(std::string_view api, bool is_floating) {
  if (api != CLAP_WINDOW_API_WIN32) {
    return false;
  }

  return true;
}

void Ogler::gui_destroy() { editor = nullptr; }

bool Ogler::gui_set_scale(double scale) { return false; }

std::optional<std::pair<uint32_t, uint32_t>> Ogler::gui_get_size() {
  return {{data.editor_w, data.editor_h}};
}

bool Ogler::gui_can_resize() { return true; }

std::optional<clap_gui_resize_hints_t> Ogler::gui_get_resize_hints() {
  return {{
      .can_resize_horizontally = true,
      .can_resize_vertically = true,
      .preserve_aspect_ratio = false,
  }};
}

bool Ogler::gui_adjust_size(uint32_t &width, uint32_t &height) { return true; }

bool Ogler::gui_set_size(uint32_t width, uint32_t height) {
  if (!editor) {
    return true;
  }
  SetWindowPos(editor->hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE);
  data.editor_w = width;
  data.editor_h = height;
  return true;
}

bool Ogler::gui_set_parent(const clap_window_t &window) {
  editor = std::make_unique<Editor>(static_cast<HWND>(window.win32), *this);
  return true;
}

bool Ogler::gui_set_transient(const clap_window_t &window) { return false; }

void Ogler::gui_suggest_title(std::string_view title) {
  SetWindowText(editor->hwnd, title.data());
}

bool Ogler::gui_show() { return true; }

bool Ogler::gui_hide() { return true; }

} // namespace ogler