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
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <sciter-js/sciter-x.h>

namespace ogler {

struct ParameterInfo {
  std::string name;
  std::string display_name;
  float default_value;
  float minimum_val;
  float maximum_val;
  float middle_value;
  float step_size;

  sciter::value to_json() const;
  void from_json(sciter::value value);
};

struct Parameter {
  ParameterInfo info;
  float value;

  sciter::value to_json() const;
  void from_json(sciter::value value);
};

struct ShaderData {
  std::vector<unsigned> spirv_code;
  std::vector<ParameterInfo> parameters;
  std::optional<int> output_width;
  std::optional<int> output_height;
};

std::variant<ShaderData, std::string>
compile_shader(const std::vector<std::pair<std::string, std::string>> &source,
               int params_binding);
} // namespace ogler