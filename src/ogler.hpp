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

#define OGLER_STRINGIZE_(x) #x
#define OGLER_STRINGIZE(x) OGLER_STRINGIZE_(x)

struct GLFWwindow;

namespace std {
template <> struct default_delete<GLFWwindow> {
  void operator()(GLFWwindow *wnd) const;
};
} // namespace std

namespace ogler {
namespace version {
constexpr int major = OGLER_VER_MAJOR;
constexpr int minor = OGLER_VER_MINOR;
constexpr int revision = OGLER_VER_REV;

constexpr const char *string =
    OGLER_STRINGIZE(OGLER_VER_MAJOR) "." OGLER_STRINGIZE(
        OGLER_VER_MINOR) "." OGLER_STRINGIZE(OGLER_VER_REV);
} // namespace version

class OglerVst final : public vst::ReaperVstPlugin<OglerVst> {
  std::unique_ptr<GLFWwindow> window;
  std::optional<gl::Program> prog;
  std::optional<gl::Texture2D> inputTexture;
  std::optional<gl::Texture2D> outputTexture;

public:
  static constexpr int num_programs = 0;
  static constexpr int num_params = 0;
  static constexpr int num_inputs = 2;
  static constexpr int num_outputs = 2;
  static constexpr int flags = vst::AEffectFlags::EffectCanReplacing;
  static constexpr int unique_id = 0xFB000001;
  static constexpr int version = 1000;

  IVideoFrame *frame{};

  OglerVst(vst::HostCallback *hostcb);
  ~OglerVst();
  std::string_view get_effect_name() noexcept override;
  std::string_view get_vendor_name() noexcept override;
  std::string_view get_product_name() noexcept override;
  std::int32_t get_vendor_version() noexcept override;

  IVideoFrame *
  video_process_frame(std::span<const double> parms, double project_time,
                      double framerate,
                      vst::FrameFormat force_format) noexcept override;
};
} // namespace ogler