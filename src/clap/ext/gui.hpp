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

#include <optional>
#include <string_view>

#include <clap/ext/gui.h>

namespace clap {
template <typename T>
concept Gui =
    requires(T &plugin, std::string_view api, bool is_floating, double scale,
             uint32_t &r_width, uint32_t &r_height, uint32_t width,
             uint32_t height, const clap_window_t &window) {
      { plugin.gui_is_api_supported(api, is_floating) } -> std::same_as<bool>;
      {
        plugin.gui_get_preferred_api()
      } -> std::same_as<std::optional<std::pair<const char *, bool>>>;
      { plugin.gui_create(api, is_floating) } -> std::same_as<bool>;
      { plugin.gui_destroy() } -> std::same_as<void>;
      { plugin.gui_set_scale(scale) } -> std::same_as<bool>;
      {
        plugin.gui_get_size()
      } -> std::same_as<std::optional<std::pair<uint32_t, uint32_t>>>;
      { plugin.gui_can_resize() } -> std::same_as<bool>;
      {
        plugin.gui_get_resize_hints()
      } -> std::same_as<std::optional<clap_gui_resize_hints_t>>;
      { plugin.gui_adjust_size(r_width, r_height) } -> std::same_as<bool>;
      { plugin.gui_set_size(width, height) } -> std::same_as<bool>;
      { plugin.gui_set_parent(window) } -> std::same_as<bool>;
      { plugin.gui_set_transient(window) } -> std::same_as<bool>;
      { plugin.gui_suggest_title(api) } -> std::same_as<void>;
      { plugin.gui_show() } -> std::same_as<bool>;
      { plugin.gui_hide() } -> std::same_as<bool>;
    };

template <Gui Plugin> struct gui {
  static constexpr const char *id = CLAP_EXT_GUI;

  template <typename Container> static const void *get_ext() {
    static clap_plugin_gui gui{
        .is_api_supported =
            [](const clap_plugin_t *plugin, const char *api, bool is_floating) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_is_api_supported(
                  std::string_view{api}, is_floating);
            },
        .get_preferred_api =
            [](const clap_plugin_t *plugin, const char **api,
               bool *is_floating) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              auto value = self->plugin_data.gui_get_preferred_api();
              if (value) {
                *api = std::get<0>(*value);
                *is_floating = std::get<1>(*value);
                return true;
              } else {
                return false;
              }
            },
        .create =
            [](const clap_plugin_t *plugin, const char *api, bool is_floating) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_create(std::string_view{api},
                                                  is_floating);
            },
        .destroy =
            [](const clap_plugin_t *plugin) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_destroy();
            },
        .set_scale =
            [](const clap_plugin_t *plugin, double scale) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_set_scale(scale);
            },
        .get_size =
            [](const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              auto value = self->plugin_data.gui_get_size();
              if (value) {
                *width = std::get<0>(*value);
                *height = std::get<1>(*value);
                return true;
              } else {
                return false;
              }
            },
        .can_resize =
            [](const clap_plugin_t *plugin) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_can_resize();
            },
        .get_resize_hints =
            [](const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              auto value = self->plugin_data.gui_get_resize_hints();
              if (value) {
                *hints = *value;
                return true;
              } else {
                return false;
              }
            },
        .adjust_size =
            [](const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_adjust_size(*width, *height);
            },
        .set_size =
            [](const clap_plugin_t *plugin, uint32_t width, uint32_t height) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_set_size(width, height);
            },
        .set_parent =
            [](const clap_plugin_t *plugin, const clap_window_t *window) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_set_parent(*window);
            },
        .set_transient =
            [](const clap_plugin_t *plugin, const clap_window_t *window) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_set_transient(*window);
            },
        .suggest_title =
            [](const clap_plugin_t *plugin, const char *title) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_suggest_title(
                  std::string_view{title});
            },
        .show =
            [](const clap_plugin_t *plugin) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_show();
            },
        .hide =
            [](const clap_plugin_t *plugin) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.gui_hide();
            },
    };
    return &gui;
  }
};
} // namespace clap