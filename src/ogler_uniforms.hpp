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

#include <array>

#include <WDL/eel2/ns-eel.h>

#include <vulkan/vulkan.hpp>

namespace ogler {

struct Uniforms {
  float iResolution_w, iResolution_h;
  float iTime;
  float iSampleRate;
  float iFrameRate;
  float iWet;
  int num_inputs;
};
static_assert(sizeof(Uniforms) < 128,
              "Keep this under 128 bytes to ensure compatibility!");

union UniformsView {
  Uniforms data;
  std::array<float, sizeof(data) / sizeof(float)> values;
};

static constexpr unsigned max_num_inputs = 64;

static constexpr uint32_t gmem_size =
    NSEEL_RAM_BLOCKS * NSEEL_RAM_ITEMSPERBLOCK;

static constexpr vk::Format RGBAFormat = vk::Format::eB8G8R8A8Unorm;

} // namespace ogler