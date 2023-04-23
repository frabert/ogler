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
#include "compile_shader.hpp"
#include "ogler_editor.hpp"

#include <mutex>
#include <nlohmann/json.hpp>

#include <nlohmann/json_fwd.hpp>
#include <reaper_plugin_functions.h>

#include <optional>
#include <sstream>
#include <utility>
#include <variant>

namespace ogler {

static constexpr int AlignTo4(int x) { return (x + 3) & 0xFFFFFFFC; }

OglerVst::OglerVst(vst::HostCallback *hostcb)
    : vst::ReaperVstPlugin<OglerVst>(hostcb) {
  if (auto err = recompile_shaders()) {
    DBG << *err << '\n';
  }
}

OglerVst::~OglerVst() = default;

std::string_view OglerVst::get_effect_name() noexcept { return "Ogler"; }
std::string_view OglerVst::get_vendor_name() noexcept {
  return "Francesco Bertolaccini";
}
std::string_view OglerVst::get_product_name() noexcept { return "Ogler"; }
std::int32_t OglerVst::get_vendor_version() noexcept { return 1000; }

void PatchData::deserialize(std::istream &s) {
  nlohmann::json obj;
  s >> obj;

  video_shader = obj["video_shader"];
}

void PatchData::serialize(std::ostream &s) {
  nlohmann::json obj{{"video_shader", video_shader}};
  s << obj;
}

void OglerVst::save_preset_data(std::ostream &s) noexcept { data.serialize(s); }

void OglerVst::load_preset_data(std::istream &s) noexcept {
  data.deserialize(s);
  recompile_shaders();
}

void OglerVst::save_bank_data(std::ostream &s) noexcept { save_preset_data(s); }

void OglerVst::load_bank_data(std::istream &s) noexcept { load_preset_data(s); }

std::optional<std::string> OglerVst::recompile_shaders() {
  auto res = compile_shader({R"(#version 460

layout(local_size_x = 1, local_size_y = 1) in;

layout(binding = 0) uniform UniformBlock {
  vec2 iResolution;
  float iTime;
  float iSampleRate;
  vec2 iChannelResolution;
  float iFrameRate;
  float iWet;
};
layout(binding = 1) uniform sampler2D iChannel;
layout(binding = 2) uniform writeonly image2D oChannel;)",
                             data.video_shader,
                             R"(void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID.xy));
    ivec2 coords = ivec2(gl_GlobalInvocationID.x, gl_NumWorkGroups.y - gl_GlobalInvocationID.y);
    imageStore(oChannel, coords, fragColor);
})"});
  if (std::holds_alternative<std::string>(res)) {
    return std::move(std::get<std::string>(res));
  }
  return {};
}

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  return nullptr;
}
} // namespace ogler