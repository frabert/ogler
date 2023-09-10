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

#include <clap/ext/params.h>
#include <mutex>
#include <optional>

#undef min

namespace ogler {

uint32_t Ogler::params_count() {
  std::unique_lock<std::recursive_mutex> lock(params_mutex,
                                              std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return 0;
  }

  return data.parameters.size();
}

std::optional<clap_param_info_t> Ogler::params_get_info(uint32_t param_index) {
  std::unique_lock<std::recursive_mutex> lock(params_mutex,
                                              std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return std::nullopt;
  }

  if (param_index >= data.parameters.size()) {
    return std::nullopt;
  }
  auto &param = data.parameters[param_index];
  clap_param_info_t res{
      .id = param_index,
      .flags = CLAP_PARAM_IS_AUTOMATABLE,
      .cookie = &param.value,
      .min_value = param.info.minimum_val,
      .max_value = param.info.maximum_val,
      .default_value = param.info.default_value,
  };
  if (param.info.step_size != 0.0f) {
    res.flags |= CLAP_PARAM_IS_STEPPED;
  }
  std::copy_n(param.info.display_name.begin(),
              std::min({static_cast<size_t>(param.info.display_name.size()),
                        static_cast<size_t>(CLAP_NAME_SIZE)}),
              res.name);
  return res;
}

std::optional<double> Ogler::params_get_value(clap_id param_id) {
  std::unique_lock<std::recursive_mutex> lock(params_mutex,
                                              std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return std::nullopt;
  }

  if (param_id >= data.parameters.size()) {
    return std::nullopt;
  }
  return data.parameters[param_id].value;
}

bool Ogler::params_value_to_text(clap_id param_id, double value,
                                 std::span<char> out_buffer) {
  std::unique_lock<std::recursive_mutex> lock(params_mutex,
                                              std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return false;
  }

  if (param_id >= data.parameters.size()) {
    return false;
  }

  std::snprintf(out_buffer.data(), out_buffer.size(), "%.2f",
                data.parameters[param_id].value);
  return true;
}

std::optional<double>
Ogler::params_text_to_value(clap_id param_id,
                            std::string_view param_value_text) {
  std::unique_lock<std::recursive_mutex> lock(params_mutex,
                                              std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return std::nullopt;
  }

  if (param_id >= data.parameters.size()) {
    return std::nullopt;
  }
  return std::stof(param_value_text.data());
}

void Ogler::params_flush(const clap_input_events_t &in,
                         const clap_output_events_t &out) {
  handle_events(in);
}

void Ogler::handle_events(const clap_input_events_t &events) {
  std::unique_lock<std::recursive_mutex> lock(params_mutex,
                                              std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return;
  }
  bool events_to_handle = false;

  for (uint32_t i = 0; i < events.size(&events); ++i) {
    auto event = events.get(&events, i);
    switch (event->type) {
    case CLAP_EVENT_PARAM_VALUE: {
      auto param_value_event =
          reinterpret_cast<const clap_event_param_value_t *>(event);
      if (param_value_event->cookie) {
        *static_cast<float *>(param_value_event->cookie) =
            param_value_event->value;
      } else {
        [[unlikely]] data.parameters[param_value_event->param_id].value =
            param_value_event->value;
      }
      events_to_handle = true;
      break;
    }
    default:
      break;
    }
  }

  if (editor && events_to_handle) {
    editor->params_changed(data.parameters);
  }
}

} // namespace ogler