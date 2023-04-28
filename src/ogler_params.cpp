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

#include "ogler_params.hpp"

namespace ogler {

void OglerVst::set_parameter(std::int32_t index, float value) noexcept {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  parameters[index].value = value;
}

float OglerVst::get_parameter(std::int32_t index) noexcept {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  return parameters[index].value;
}

int OglerVst::get_num_parameters() noexcept {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  return parameters.size();
}

void OglerVst::get_param_range(int index, double &min, double &max) {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  min = parameters[index].info.minimum_val;
  max = parameters[index].info.maximum_val;
}

std::string_view OglerVst::get_parameter_label(int index) noexcept {
  return "";
}

std::string_view OglerVst::get_parameter_text(int index) noexcept {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  param_text = std::to_string(parameters[index].value);
  return param_text;
}

std::string_view OglerVst::get_parameter_name(int index) noexcept {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  return parameters[index].info.display_name;
}

bool OglerVst::can_be_automated(int index) noexcept { return true; }

std::optional<vst::ParameterProperties>
OglerVst::get_parameter_properties(int index) noexcept {
  std::unique_lock<std::recursive_mutex> lock(params_mutex);

  auto &info = parameters[index].info;
  vst::ParameterProperties props{
      .stepFloat = info.step_size,
      .smallStepFloat = info.step_size,
      .largeStepFloat = info.step_size,
      .label{},
      .flags = info.step_size != 0 ? vst::ParameterUsesFloatStep
                                   : static_cast<vst::ParameterFlags>(0),
      .shortLabel{},
  };

  copy_string(info.name.data(), props.label.data(), info.name.size(),
              props.label.size());
  copy_string(info.name.data(), props.shortLabel.data(), info.name.size(),
              props.shortLabel.size());
  return props;
}

} // namespace ogler