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

#include "opengl.h"
#include "vst/ReaperVstPlugin.hpp"

#include <memory>
#include <mutex>

#define OGLER_STRINGIZE_(x) #x
#define OGLER_STRINGIZE(x) OGLER_STRINGIZE_(x)

struct GLFWwindow;

namespace std {
template <> struct default_delete<GLFWwindow> {
  void operator()(GLFWwindow *wnd) const;
};
} // namespace std

namespace ogler {
HINSTANCE get_hinstance();

class DebugStream {
public:
  template <typename T> DebugStream operator<<(T &&value) {
    std::stringstream ss;
    ss << std::forward<T>(value);
    OutputDebugStringA(ss.str().c_str());
    return {};
  }
};

class ReaperStream {
public:
  template <typename T> ReaperStream operator<<(T &&value) {
    std::stringstream ss;
    ss << std::forward<T>(value);
    ShowConsoleMsg(ss.str().c_str());
    return {};
  }
};

#define DBG DebugStream()

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
  std::unique_ptr<GLFWwindow> window_for_video;
  std::optional<gl::Program> prog;
  std::optional<gl::Texture2D> inputTexture;
  std::optional<gl::Texture2D> outputTexture;
  std::unique_ptr<IVideoFrame> output_frame, input_frame;

  struct Editor;
  int editor_w{1024};
  int editor_h{768};
  std::unique_ptr<Editor> editor;

  std::mutex video_ctx_mtx;

  PatchData data;

  std::optional<std::string> recompile_shaders();

public:
  static constexpr int num_programs = 0;
  static constexpr int num_params = 0;
  static constexpr int num_inputs = 2;
  static constexpr int num_outputs = 2;
  static constexpr int flags = vst::AEffectFlags::EffectCanReplacing |
                               vst::AEffectFlags::EffectHasEditor;
  static constexpr int unique_id = 0xFB000001;
  static constexpr int version = 1000;

  OglerVst(vst::HostCallback *hostcb);
  ~OglerVst();

protected:
  std::string_view get_effect_name() noexcept final;
  std::string_view get_vendor_name() noexcept final;
  std::string_view get_product_name() noexcept final;
  std::int32_t get_vendor_version() noexcept final;

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
};
} // namespace ogler