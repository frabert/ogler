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

#include <memory>
#include <mutex>

#include <WDL/wdltypes.h>
#include <reaper_plugin.h>
#include <video_frame.h>
#include <video_processor.h>

#include <clap/events.h>
#include <clap/ext/gui.h>
#include <clap/ext/params.h>
#include <clap/host.h>
#include <clap/plugin.h>

#include "vulkan_context.hpp"

#define OGLER_STRINGIZE_(x) #x
#define OGLER_STRINGIZE(x) OGLER_STRINGIZE_(x)

namespace ogler {

enum class FrameFormat : int {
  Default = 0,
  YV12 = 'YV12',
  YUV2 = 'YUV2',
  RGBA = 'RGBA',
};

HINSTANCE get_hinstance();

namespace version {
constexpr int major = OGLER_VER_MAJOR;
constexpr int minor = OGLER_VER_MINOR;
constexpr int revision = OGLER_VER_REV;
constexpr const char *string =
    OGLER_STRINGIZE(OGLER_VER_MAJOR) "." OGLER_STRINGIZE(
        OGLER_VER_MINOR) "." OGLER_STRINGIZE(OGLER_VER_REV);
} // namespace version

using eel_gmem_attach_f = double ***(const char *name, bool is_alloc);
using mutex_stub_f = void();

class EELMutex {
  mutex_stub_f *enter;
  mutex_stub_f *leave;

public:
  EELMutex(mutex_stub_f *enter, mutex_stub_f *leave)
      : enter(enter), leave(leave) {}

  void lock() { enter(); }
  void unlock() { leave(); }
};

struct PatchData {
  static constexpr int default_editor_w = 1024;
  static constexpr int default_editor_h = 768;
  static constexpr int default_editor_zoom = 1;

  std::string video_shader{
      R"(void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord / iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));

    // Output to screen
    fragColor = vec4(col, 1.0);
})"};
  int editor_w = default_editor_w;
  int editor_h = default_editor_h;
  int editor_zoom = default_editor_zoom;

  void deserialize(std::istream &);
  void serialize(std::ostream &);
};

struct SharedVulkan {
  VulkanContext vulkan;

  Buffer<float> gmem_transfer_buffer;
  Buffer<float> gmem_buffer;

  SharedVulkan();
};

struct InputImage {
  Image image;
  Buffer<char> transfer_buffer;
  vk::raii::ImageView view;
};

class Ogler final {
  const clap_host_t &host;
  std::unique_ptr<IREAPERVideoProcessor> vproc;

  int output_width = 1024;
  int output_height = 768;

  static SharedVulkan &get_shared_vulkan();

  SharedVulkan &shared;
  vk::raii::Sampler sampler;
  vk::raii::CommandBuffer command_buffer;
  vk::raii::Queue queue;
  vk::raii::Fence fence;

  Buffer<char> output_transfer_buffer;
  Image output_image;
  vk::raii::ImageView output_image_view;
  Image previous_image;
  vk::raii::ImageView previous_image_view;

  InputImage empty_input;
  std::vector<InputImage> input_images;

  std::optional<Buffer<float>> params_buffer;

  Buffer<std::pair<float, float>> input_resolution_buffer;

  struct Compute;
  std::unique_ptr<Compute> compute;

  IVideoFrame *output_frame{};

  struct Editor;
  std::unique_ptr<Editor> editor;

  PatchData data;

  struct Parameter;
  std::vector<Parameter> parameters;
  std::string param_text;

  std::optional<std::string> recompile_shaders();

  std::mutex video_mutex;
  std::recursive_mutex params_mutex;

  std::optional<EELMutex> eel_mutex;
  double ***gmem{};

  InputImage create_input_image(int w, int h);

  template <typename Func> void one_shot_execute(Func f) {
    {
      vk::CommandBufferBeginInfo begin_info{
          .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
      };
      command_buffer.begin(begin_info);
    }
    f();
    command_buffer.end();

    vk::SubmitInfo SubmitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &*command_buffer,
    };
    queue.submit({SubmitInfo}, *fence);
    auto res = shared.vulkan.device.waitForFences({*fence}, // List of fences
                                                  true,     // Wait All
                                                  uint64_t(-1)); // Timeout
    assert(res == vk::Result::eSuccess);
    shared.vulkan.device.resetFences({*fence});
    command_buffer.reset();
  }

  template <typename T> T *get_reaper_function(std::string_view name) {
    auto reaper_plugin = static_cast<const reaper_plugin_info_t *>(
        host.get_extension(&host, "cockos.reaper_extension"));
    return reinterpret_cast<T *>(reaper_plugin->GetFunc(name.data()));
  }

  IVideoFrame *video_process_frame(std::span<const double> parms,
                                   double project_time, double framerate,
                                   FrameFormat force_format) noexcept;

  void handle_events(const clap_input_events_t &events);

public:
  static constexpr const char *id = "dev.bertolaccini.ogler";
  static constexpr const char *name = "ogler";
  static constexpr const char *vendor = "Francesco Bertolaccini";
  static constexpr const char *url = "https://github.com/frabert/ogler";
  static constexpr const char *manual_url =
      "https://github.com/frabert/ogler/blob/main/docs/Reference.md";
  static constexpr const char *support_url =
      "https://github.com/frabert/ogler/discussions";
  static constexpr const char *version = version::string;
  static constexpr const char *description = "Use GLSL video shaders in REAPER";
  static constexpr const char *features[] = {"reaper:video-processor", {}};

  Ogler(const clap_host_t &host);
  ~Ogler();

  bool init();
  bool activate(double sample_rate, uint32_t min_frames_count,
                uint32_t max_frames_count);
  void deactivate();

  bool start_processing();
  void stop_processing();
  void reset();
  clap_process_status process(const clap_process_t &process);

  bool state_save(std::ostream &os);
  bool state_load(std::istream &os);

  void *get_extension(std::string_view id);
  void on_main_thread();

  uint32_t params_count();
  std::optional<clap_param_info_t> params_get_info(uint32_t param_index);
  std::optional<double> params_get_value(clap_id param_id);
  bool params_value_to_text(clap_id param_id, double value,
                            std::span<char> out_buffer);
  std::optional<double> params_text_to_value(clap_id param_id,
                                             std::string_view param_value_text);
  void params_flush(const clap_input_events_t &in,
                    const clap_output_events_t &out);

  bool gui_is_api_supported(std::string_view api, bool is_floating);
  std::optional<std::pair<const char *, bool>> gui_get_preferred_api();
  bool gui_create(std::string_view api, bool is_floating);
  void gui_destroy();
  bool gui_set_scale(double scale);
  std::optional<std::pair<uint32_t, uint32_t>> gui_get_size();
  bool gui_can_resize();
  std::optional<clap_gui_resize_hints_t> gui_get_resize_hints();
  bool gui_adjust_size(uint32_t &width, uint32_t &height);
  bool gui_set_size(uint32_t width, uint32_t height);
  bool gui_set_parent(const clap_window_t &window);
  bool gui_set_transient(const clap_window_t &window);
  void gui_suggest_title(std::string_view title);
  bool gui_show();
  bool gui_hide();
};
} // namespace ogler