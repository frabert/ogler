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
#include "gl_context_lock.hpp"
#include "ogler_editor.hpp"

#include <mutex>
#include <nlohmann/json.hpp>

#include "opengl.h"

#include <GLFW/glfw3.h>
#include <gl.h>
#include <nlohmann/json_fwd.hpp>
#include <reaper_plugin_functions.h>

#include <optional>
#include <sstream>
#include <utility>
#include <variant>

void std::default_delete<GLFWwindow>::operator()(GLFWwindow *wnd) const {
  glfwDestroyWindow(wnd);
}

namespace ogler {

static constexpr int AlignTo4(int x) { return (x + 3) & 0xFFFFFFFC; }

static void DebugCallback(GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
  DBG << "GL error: " << message << '\n';
}

static void glfw_error_callback(int error, const char *description) {
  DBG << "GLFW error " << error << ": " << description << '\n';
}

static std::variant<gl::Program, std::string>
CreateProgram(const std::string &source) {
  auto shader = gl::Shader::compile(gl::ShaderKind::Compute, {R"(#version 460

layout(local_size_x = 1, local_size_y = 1) in;

uniform vec2 iResolution;
uniform float iTime;
uniform float iSampleRate;
uniform vec2 iChannelResolution;
uniform sampler2D iChannel;
uniform writeonly image2D oChannel;
uniform float iFrameRate;
uniform float iWet;)",
                                                              source,
                                                              R"(void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID));
    imageStore(oChannel, ivec2(gl_GlobalInvocationID.xy), fragColor);
})"});

  if (std::holds_alternative<std::string>(shader)) {
    return std::move(std::get<std::string>(shader));
  }
  auto prog = gl::Program::link(std::get<gl::Shader>(shader));

  if (std::holds_alternative<std::string>(prog)) {
    return std::move(std::get<std::string>(prog));
  }
  return std::move(std::get<gl::Program>(prog));
}

OglerVst::OglerVst(vst::HostCallback *hostcb)
    : vst::ReaperVstPlugin<OglerVst>(hostcb) {
  glfwInit();
  glfwSetErrorCallback(glfw_error_callback);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  video.window = std::unique_ptr<GLFWwindow>(
      glfwCreateWindow(100, 100, "", nullptr, nullptr));
  if (auto err = recompile_shaders()) {
    DBG << *err << '\n';
  }
}

OglerVst::~OglerVst() {
  // Need to make sure we bring the context back to the right thread before
  // destroying all of the OpenGL objects
  glfwMakeContextCurrent(video.window.get());
}

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
  GlContextLock mtx(*video.window);
  std::unique_lock<GlContextLock> lock(mtx);

  auto video_prog = CreateProgram(data.video_shader);
  std::optional<std::string> res{};
  if (std::holds_alternative<std::string>(video_prog)) {
    res = std::make_optional<std::string>(
        std::move(std::get<std::string>(video_prog)));
  } else {
    video.prog = std::move(std::get<gl::Program>(video_prog));

    video.iResolution = video.prog->getUniform<gl::vec2>("iResolution");
    video.iTime = video.prog->getUniform<float>("iTime");
    video.iSampleRate = video.prog->getUniform<float>("iSampleRate");
    video.iChannelResolution =
        video.prog->getUniform<gl::vec2>("iChannelResolution");
    video.iChannel = video.prog->getUniform<int>("iChannel");
    video.oChannel = video.prog->getUniform<int>("oChannel");
    video.iFrameRate = video.prog->getUniform<float>("iFrameRate");
    video.iWet = video.prog->getUniform<float>("iWet");
  }

  return res;
}

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  GlContextLock mtx(*video.window);
  std::unique_lock<GlContextLock> lock(mtx);
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(DebugCallback, nullptr);
  if (!video.prog) {
    return nullptr;
  }
  constexpr int out_w = 1024;
  constexpr int out_h = 768;
  if (!video.output_frame) {
    video.output_frame = std::unique_ptr<IVideoFrame>(
        new_video_frame(out_w, out_h, vst::FrameFormat::RGBA));
  }
  if (get_video_num_inputs() > 0) {
    auto input_frame = get_video_input(0, vst::FrameFormat::RGBA);
    if (!video.input_texture ||
        video.input_texture->get_width(0) != input_frame->get_w() ||
        video.input_texture->get_height(0) != input_frame->get_h()) {
      video.input_texture =
          gl::Texture2D::create(gl::InternalFormat::RGBA8, input_frame->get_w(),
                                input_frame->get_h());
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, AlignTo4(input_frame->get_w()));
    video.input_texture->upload(0, 0, 0, input_frame->get_w(),
                                input_frame->get_h(), gl::PixelFormat::BGRA,
                                gl::PixelType::UByte, input_frame->get_bits());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }

  video.prog->use();
  if (video.iResolution) {
    *video.iResolution = gl::vec2{out_w, out_h};
  }
  if (video.iTime) {
    *video.iTime = project_time;
  }
  if (video.iSampleRate) {
    *video.iSampleRate = host_get_sample_rate();
  }
  if (video.input_texture) {
    if (video.iChannelResolution) {
      *video.iChannelResolution =
          gl::vec2{float(video.input_texture->get_width(0)),
                   float(video.input_texture->get_height(0))};
    }
    if (video.iChannel) {
      *video.iChannel = 0;
    }
    video.input_texture->bindTextureUnit(0);
  }

  if (video.oChannel) {
    *video.oChannel = 1;
  }

  if (video.iFrameRate) {
    *video.iFrameRate = framerate;
  }

  if (video.iWet) {
    *video.iWet = parms[0];
  }

  if (!video.output_texture) {
    video.output_texture =
        gl::Texture2D::create(gl::InternalFormat::RGBA8, out_w, out_h);
  }
  video.output_texture->bindImageUnit(1, 0, false, 0, gl::Access::WriteOnly,
                                      gl::InternalFormat::RGBA8);

  glDispatchCompute(out_w, out_h, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  glPixelStorei(GL_PACK_ROW_LENGTH, AlignTo4(video.output_frame->get_w()));
  video.output_texture->download(0, gl::PixelFormat::BGRA, gl::PixelType::UByte,
                                 video.output_frame->get_rowspan() *
                                     video.output_frame->get_h(),
                                 video.output_frame->get_bits());
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);

  return video.output_frame.get();
}
} // namespace ogler