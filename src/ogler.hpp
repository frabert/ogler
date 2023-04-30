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

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "vst/ReaperVstPlugin.hpp"

#include <memory>
#include <mutex>

#include "vulkan_context.hpp"

#define OGLER_STRINGIZE_(x) #x
#define OGLER_STRINGIZE(x) OGLER_STRINGIZE_(x)

namespace ogler {
HINSTANCE get_hinstance();

namespace version {
constexpr int major = OGLER_VER_MAJOR;
constexpr int minor = OGLER_VER_MINOR;
constexpr int revision = OGLER_VER_REV;

constexpr const char *string =
    OGLER_STRINGIZE(OGLER_VER_MAJOR) "." OGLER_STRINGIZE(
        OGLER_VER_MINOR) "." OGLER_STRINGIZE(OGLER_VER_REV);
} // namespace version

struct PatchData {
  std::string video_shader{
      R"(void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord / iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));

    // Output to screen
    fragColor = vec4(col, 1.0);
})"};

  void deserialize(std::istream &);
  void serialize(std::ostream &);
};

class OglerVst final : public vst::ReaperVstPlugin<OglerVst> {
  int output_width = 1024;
  int output_height = 768;

  VulkanContext vulkan;
  vk::raii::Sampler sampler;
  vk::raii::CommandBuffer command_buffer;
  vk::raii::Queue queue;
  vk::raii::Fence fence;

  Buffer output_transfer_buffer;
  Image output_image;
  vk::raii::ImageView output_image_view;

  std::optional<Buffer> input_transfer_buffer;
  std::optional<Image> input_image;
  std::optional<vk::raii::ImageView> input_image_view;

  std::optional<Buffer> params_buffer;

  struct Compute;
  std::unique_ptr<Compute> compute;

  IVideoFrame *output_frame{};

  struct Editor;
  int editor_w{1024};
  int editor_h{768};
  std::unique_ptr<Editor> editor;

  PatchData data;

  struct Parameter;
  std::vector<Parameter> parameters;
  std::string param_text;

  std::optional<std::string> recompile_shaders();

  std::mutex video_mutex;
  std::recursive_mutex params_mutex;

public:
  static constexpr int num_programs = 0;
  static constexpr int num_params = 0;
  static constexpr int num_inputs = 2;
  static constexpr int num_outputs = 2;
  static constexpr int flags = vst::AEffectFlags::EffectCanReplacing |
                               vst::AEffectFlags::EffectHasEditor |
                               vst::AEffectFlags::EffectProgramChunks;
  static constexpr int unique_id = 0xFB000001;
  static constexpr int version = 1000;

  OglerVst(vst::HostCallback *hostcb);
  ~OglerVst();

protected:
  std::string_view get_effect_name() noexcept final;
  std::string_view get_vendor_name() noexcept final;
  std::string_view get_product_name() noexcept final;
  std::int32_t get_vendor_version() noexcept final;

  void process(float **inputs, float **outputs,
               std::int32_t num_samples) noexcept final;
  void process(double **inputs, double **outputs,
               std::int32_t num_samples) noexcept final;

  bool has_editor() noexcept final;
  void get_editor_bounds(std::int16_t &top, std::int16_t &left,
                         std::int16_t &bottom,
                         std::int16_t &right) noexcept final;
  void open_editor(void *hWnd) noexcept final;
  void close_editor() noexcept final;
  bool is_editor_open() noexcept final;

  IVideoFrame *
  video_process_frame(std::span<const double> parms, double project_time,
                      double framerate,
                      vst::FrameFormat force_format) noexcept final;

  void save_preset_data(std::ostream &s) noexcept final;
  void load_preset_data(std::istream &s) noexcept final;
  void save_bank_data(std::ostream &s) noexcept final;
  void load_bank_data(std::istream &s) noexcept final;

  void set_parameter(std::int32_t index, float value) noexcept final;
  float get_parameter(std::int32_t index) noexcept final;
  int get_num_parameters() noexcept final;
  void get_param_range(int index, double &min, double &max) final;
  std::string_view get_parameter_label(int index) noexcept final;
  std::string_view get_parameter_text(int index) noexcept final;
  std::string_view get_parameter_name(int index) noexcept final;
  bool can_be_automated(int index) noexcept final;
  std::optional<vst::ParameterProperties>
  get_parameter_properties(int index) noexcept final;
};
} // namespace ogler