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
#include <span>
#include <string_view>

#include <clap/ext/params.h>

namespace clap {
template <typename T>
concept Params =
    requires(T &plugin, uint32_t param_index, clap_id param_id, double value,
             std::span<char> text_out, std::string_view text,
             const clap_input_events_t &in, const clap_output_events_t &out) {
      { plugin.params_count() } -> std::same_as<uint32_t>;
      {
        plugin.params_get_info(param_index)
      } -> std::same_as<std::optional<clap_param_info_t>>;
      {
        plugin.params_get_value(param_id)
      } -> std::same_as<std::optional<double>>;
      {
        plugin.params_value_to_text(param_id, value, text_out)
      } -> std::same_as<bool>;
      {
        plugin.params_text_to_value(param_id, text)
      } -> std::same_as<std::optional<double>>;
      { plugin.params_flush(in, out) } -> std::same_as<void>;
    };

template <Params Plugin> struct params {
  static constexpr const char *id = CLAP_EXT_PARAMS;

  template <typename Container> struct impl {
    static const void *get() {
      static clap_plugin_params_t params{
          .count =
              [](const clap_plugin_t *plugin) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                return self->plugin_data.params_count();
              },
          .get_info =
              [](const clap_plugin_t *plugin, uint32_t param_index,
                 clap_param_info_t *param_info) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                auto info = self->plugin_data.params_get_info(param_index);
                if (info) {
                  *param_info = *info;
                  return true;
                } else {
                  return false;
                }
              },
          .get_value =
              [](const clap_plugin_t *plugin, clap_id param_id,
                 double *out_value) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                auto value = self->plugin_data.params_get_value(param_id);
                if (value) {
                  *out_value = *value;
                  return true;
                } else {
                  return false;
                }
              },
          .value_to_text =
              [](const clap_plugin_t *plugin, clap_id param_id, double value,
                 char *out_buffer, uint32_t out_buffer_capacity) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                return self->plugin_data.params_value_to_text(
                    param_id, value,
                    std::span<char>(out_buffer, out_buffer_capacity));
              },
          .text_to_value =
              [](const clap_plugin_t *plugin, clap_id param_id,
                 const char *param_value_text, double *out_value) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                std::string_view text{param_value_text};
                auto value =
                    self->plugin_data.params_text_to_value(param_id, text);
                if (value) {
                  *out_value = *value;
                  return true;
                } else {
                  return false;
                }
              },
          .flush =
              [](const clap_plugin_t *plugin, const clap_input_events_t *in,
                 const clap_output_events_t *out) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                return self->plugin_data.params_flush(*in, *out);
              },
      };
      return &params;
    }
  };
};
} // namespace clap