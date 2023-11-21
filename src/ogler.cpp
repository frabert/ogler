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
#include "compile_shader.hpp"
#include "ogler_compute.hpp"
#include "ogler_editor.hpp"
#include "ogler_preferences.hpp"
#include "ogler_uniforms.hpp"
#include "string_utils.hpp"

#include <clap/events.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/params.h>
#include <clap/process.h>

#include <sciter-js/value.hpp>

#include <reaper_plugin_functions.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace ogler {

SharedVulkan::SharedVulkan()
    : gmem_transfer_buffer(vulkan.create_buffer<float>(
          {}, gmem_size, vk::BufferUsageFlagBits::eTransferSrc,
          vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      gmem_buffer(vulkan.create_buffer<float>(
          {}, gmem_size,
          vk::BufferUsageFlagBits::eTransferDst |
              vk::BufferUsageFlagBits::eStorageBuffer,
          vk::SharingMode::eExclusive, vk::MemoryPropertyFlagBits::eDeviceLocal,
          false)) {}

int Ogler::get_output_width() {
  if (shader_output_width.has_value()) {
    return *shader_output_width;
  } else {
    auto [w, h] = reaper->get_current_project_size(fallback_output_width,
                                                   fallback_output_height);
    return w;
  }
}

int Ogler::get_output_height() {
  if (shader_output_height.has_value()) {
    return *shader_output_height;
  } else {
    auto [w, h] = reaper->get_current_project_size(fallback_output_width,
                                                   fallback_output_height);
    return h;
  }
}

Ogler::Ogler(const clap::host &host)
    : host(host), reaper(IReaper::get_reaper(host)),
      shared(get_shared_vulkan()),
      command_buffer(shared.vulkan.create_command_buffer()),
      queue(shared.vulkan.get_queue(0)), fence(shared.vulkan.create_fence()),
      sampler(shared.vulkan.create_sampler()),
      output_transfer_buffer(shared.vulkan.create_buffer<char>(
          {}, get_output_width() * get_output_height() * 4,
          vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      output_image(
          shared.vulkan.create_image(get_output_width(), get_output_height(),
                                     RGBAFormat, vk::ImageTiling::eOptimal,
                                     vk::ImageUsageFlagBits::eStorage |
                                         vk::ImageUsageFlagBits::eTransferSrc |
                                         vk::ImageUsageFlagBits::eSampled)),
      output_image_view(
          shared.vulkan.create_image_view(output_image, RGBAFormat)),
      previous_image(
          shared.vulkan.create_image(get_output_width(), get_output_height(),
                                     RGBAFormat, vk::ImageTiling::eOptimal,
                                     vk::ImageUsageFlagBits::eStorage |
                                         vk::ImageUsageFlagBits::eTransferSrc |
                                         vk::ImageUsageFlagBits::eSampled)),
      previous_image_view(
          shared.vulkan.create_image_view(previous_image, RGBAFormat)),
      empty_input(create_input_image(1, 1)),
      input_resolution_buffer(
          shared.vulkan.create_buffer<std::pair<float, float>>(
              {}, max_num_inputs, vk::BufferUsageFlagBits::eUniformBuffer,
              vk::SharingMode::eExclusive,
              vk::MemoryPropertyFlagBits::eHostCoherent |
                  vk::MemoryPropertyFlagBits::eHostVisible)) {
  static std::mutex pref_mtx;
  static const char *ini_file = nullptr;
  static prefs_page_register_t pref_page = {
      .idstr = "ogler",
      .displayname = "ogler",
      .create = [](HWND parent) -> HWND {
        if (!ini_file) {
          return nullptr;
        }
        return PreferencesWindow::create(parent, get_hinstance(), 100, 100,
                                         TEXT("ogler preferences"), ini_file);
      },
      .par_id = 0x9a,
      .par_idstr = "",
      .childrenFlag = 0,
      .treeitem = nullptr,
      .hwndCache = nullptr,
      ._extra = {},
  };
  std::unique_lock<std::mutex> lock(pref_mtx);
  if (!ini_file) {
    ini_file = reaper->get_ini_file();
    reaper->plugin_register("prefpage", &pref_page);
  }
}

Ogler::~Ogler() {
  std::unique_lock<std::mutex> lock(video_mutex);
  vproc = nullptr;
}

bool Ogler::activate(double sample_rate, uint32_t min_frames_count,
                     uint32_t max_frames_count) {
  compiler_error = recompile_shaders();

  if (!compiler_error.has_value()) {
    if (editor) {
      editor->params_changed(data.parameters);
    }
    vproc = reaper->create_video_processor();
    vproc->userdata = this;
    vproc->process_frame =
        [](IREAPERVideoProcessor *vproc, const double *parmlist, int nparms,
           double project_time, double frate, int force_format) {
          auto plugin = static_cast<Ogler *>(vproc->userdata);
          return plugin->video_process_frame(
              std::span{parmlist, static_cast<size_t>(nparms)}, project_time,
              frate, static_cast<FrameFormat>(force_format));
        };
    vproc->get_parameter_value = [](IREAPERVideoProcessor *vproc, int idx,
                                    double *valueOut) -> bool {
      auto plugin = static_cast<Ogler *>(vproc->userdata);
      auto val = plugin->params_get_value(static_cast<clap_id>(idx));
      if (val.has_value()) {
        *valueOut = *val;
        return true;
      } else {
        return false;
      }
    };
  } else if (editor) {
    editor->compiler_error(*compiler_error);
  }
  return true;
}
void Ogler::deactivate() {
  std::unique_lock<std::mutex> lock(video_mutex);
  vproc = nullptr;
}

bool Ogler::start_processing() { return true; }

void Ogler::stop_processing() {}

void Ogler::reset() {}

clap_process_status Ogler::process(const clap_process_t &process) {
  handle_events(*process.in_events);
  if (process.audio_inputs->data64) {
    std::copy_n(process.audio_inputs->data64[0], process.frames_count,
                process.audio_outputs->data64[0]);
    std::copy_n(process.audio_inputs->data64[1], process.frames_count,
                process.audio_outputs->data64[1]);
  } else {
    std::copy_n(process.audio_inputs->data32[0], process.frames_count,
                process.audio_outputs->data32[0]);
    std::copy_n(process.audio_inputs->data32[1], process.frames_count,
                process.audio_outputs->data32[1]);
  }
  return CLAP_PROCESS_CONTINUE;
}

void *Ogler::get_extension(std::string_view id) { return nullptr; }

void Ogler::on_main_thread() {}

void PatchData::deserialize(const clap::istream &s) {
  std::string json_str;
  json_str.resize(4096);
  size_t sz = 0;
  while (true) {
    if (json_str.size() <= sz) {
      json_str.resize(json_str.size() * 2);
    }
    auto to_be_read = json_str.size() - sz;
    auto read = s.read(json_str.data() + sz, to_be_read);
    sz += read;
    if (read < to_be_read) {
      json_str.resize(sz);
      break;
    }
  }
  auto json_wstr = to_wstring(json_str);
  auto obj = sciter::value::from_string(json_wstr, CVT_JSON_LITERAL);

  video_shader = to_string(obj.get_item("video_shader").get(L""));

  do {
    auto editor_data = obj.get_item("editor");
    if (editor_data.is_nothing()) {
      editor_w = default_editor_w;
      editor_h = default_editor_h;
      break;
    }

    editor_w = editor_data.get_item("width").get(default_editor_w);
    editor_h = editor_data.get_item("height").get(default_editor_h);
  } while (false);

  auto params = obj.get_item("parameters");
  if (params.is_nothing()) {
    for (auto &param : parameters) {
      param = {};
    }
  } else {
    parameters.resize(params.length());
    for (int i = 0; i < params.length(); ++i) {
      parameters[i].from_json(params.get_item(i));
    }
  }
}

void PatchData::serialize(const clap::ostream &s) {
  std::vector<sciter::value> params;
  std::transform(parameters.begin(), parameters.end(),
                 std::back_inserter(params),
                 [](auto &param) { return param.to_json(); });

  sciter::value obj = sciter::value::make_map({
      {"video_shader", video_shader},
      {
          "editor",
          sciter::value::make_map({
              {"width", editor_w},
              {"height", editor_h},
          }),
      },
      {"parameters", sciter::value::make_array(params.size(), params.data())},
  });
  auto wstr = obj.to_string();
  auto str = to_string(wstr);
  std::string_view view = str;
  while (view.size()) {
    auto wrote = s.write(view.data(), view.size());
    view.remove_prefix(wrote);
  }
}

bool Ogler::state_save(const clap::ostream &s) {
  data.serialize(s);
  return true;
}

bool Ogler::state_load(const clap::istream &s) {
  data.deserialize(s);
  if (editor) {
    editor->reload_source();
  }
  host.request_restart();
  return true;
}

std::optional<std::string> Ogler::recompile_shaders() {
  std::unique_lock<std::mutex> video_lock(video_mutex);
  std::unique_lock<std::recursive_mutex> params_lock(params_mutex);

  auto res = compile_shader({{"<preamble>", R"(#version 460
#define OGLER_PARAMS_BINDING 0
#define OGLER_PARAMS layout(binding = OGLER_PARAMS_BINDING) uniform Params

layout (constant_id = 0) const uint ogler_gmem_size = 0;
layout (constant_id = 1) const int ogler_version_maj = 0;
layout (constant_id = 2) const int ogler_version_min = 0;
layout (constant_id = 3) const int ogler_version_rev = 0;

layout(local_size_x = 1, local_size_y = 1) in;

layout(push_constant) uniform UniformBlock {
  vec2 iResolution;
  float iTime;
  float iSampleRate;
  float iFrameRate;
  float iWet;
  int ogler_num_inputs;
};
layout(binding = 1) uniform sampler2D iChannel[];
layout(binding = 2, rgba8) uniform writeonly image2D oChannel;
layout(binding = 3) buffer readonly Gmem {
  float gmem[];
};
layout(binding = 4) uniform InputSizes {
  vec2 iChannelResolution[];
};
layout(binding = 5) uniform sampler2D ogler_previous_frame;
)"},
                             {"<source>", data.video_shader},
                             {"<epilogue>", R"(void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID));
    imageStore(oChannel, ivec2(gl_GlobalInvocationID), fragColor);
})"}},
                            /*params_binding=*/0);
  if (std::holds_alternative<std::string>(res)) {
    return std::move(std::get<std::string>(res));
  }

  auto shader_data = std::move(std::get<ShaderData>(res));

  size_t old_num = data.parameters.size();
  data.parameters.resize(shader_data.parameters.size());
  for (size_t i = 0; i < shader_data.parameters.size(); ++i) {
    auto &param = shader_data.parameters[i];
    data.parameters[i].info = param;
    if (i >= old_num) {
      data.parameters[i].value = param.default_value;
    }
  }

  try {
    compute = std::make_unique<Compute>(shared.vulkan, shader_data.spirv_code);
  } catch (vk::Error &e) {
    return e.what();
  }

  if (data.parameters.size()) {
    params_buffer = shared.vulkan.create_buffer<float>(
        {}, data.parameters.size(), vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive,
        vk::MemoryPropertyFlagBits::eHostCoherent |
            vk::MemoryPropertyFlagBits::eHostVisible);
  } else {
    params_buffer = std::nullopt;
  }

  host.params_rescan(CLAP_PARAM_RESCAN_ALL);
  return std::nullopt;
}

uint32_t Ogler::audio_ports_count(bool is_input) { return 1; }

std::optional<clap_audio_port_info_t> Ogler::audio_ports_get(uint32_t index,
                                                             bool is_input) {
  if (index >= 1) {
    return std::nullopt;
  }
  return {{
      .id = 0,
      .flags = CLAP_AUDIO_PORT_IS_MAIN | CLAP_AUDIO_PORT_SUPPORTS_64BITS,
      .channel_count = 2,
      .port_type = CLAP_PORT_STEREO,
      .in_place_pair = 0,
  }};
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

void Ogler::gui_destroy() {
  DestroyWindow(editor);
  editor = {};
}

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
  SetWindowPos(editor, nullptr, 0, 0, width, height, SWP_NOMOVE);
  data.editor_w = width;
  data.editor_h = height;
  return true;
}

class OglerEditorInterface final : public EditorInterface {
  Ogler &plugin;

public:
  OglerEditorInterface(Ogler &plugin) : plugin(plugin) {}

  void recompile_shaders() final { plugin.host.request_restart(); }

  void set_shader_source(const std::string &source) final {
    plugin.data.video_shader = source;
    plugin.host.state_mark_dirty();
  }

  const std::string &get_shader_source() final {
    return plugin.data.video_shader;
  }
  int get_zoom() final { return plugin.data.editor_zoom; }

  void set_zoom(int zoom) final { plugin.data.editor_zoom = zoom; }

  int get_width() final { return plugin.data.editor_w; }
  int get_height() final { return plugin.data.editor_h; }

  void set_width(int w) final {
    plugin.data.editor_w = w;
    plugin.host.state_mark_dirty();
  }

  void set_height(int h) final {
    plugin.data.editor_h = h;
    plugin.host.state_mark_dirty();
  }

  void set_parameter(size_t index, float value) final {
    std::unique_lock<std::mutex> lock(plugin.video_mutex);
    plugin.data.parameters[index].value = value;
    plugin.host.params_rescan(CLAP_PARAM_RESCAN_VALUES);
  }

  const char *get_ini_file() final { return plugin.reaper->get_ini_file(); }
};

bool Ogler::gui_set_parent(const clap_window_t &window) {
  editor = Editor::create(static_cast<HWND>(window.win32), get_hinstance(),
                          data.editor_w, data.editor_h, TEXT("ogler"),
                          std::make_unique<OglerEditorInterface>(*this));
  if (compiler_error) {
    editor->compiler_error(*compiler_error);
  } else {
    editor->params_changed(data.parameters);
  }
  return true;
}

bool Ogler::gui_set_transient(const clap_window_t &window) { return false; }

void Ogler::gui_suggest_title(std::string_view title) {
  SetWindowText(editor, title.data());
}

bool Ogler::gui_show() { return true; }

bool Ogler::gui_hide() { return true; }
} // namespace ogler