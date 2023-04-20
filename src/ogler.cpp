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
uniform writeonly image2D oChannel;)",
                                                              source,
                                                              R"(void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID.xy));
    ivec2 coords = ivec2(gl_GlobalInvocationID.x, gl_NumWorkGroups.y - gl_GlobalInvocationID.y);
    imageStore(oChannel, coords, fragColor);
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
    : vst::ReaperVstPlugin<OglerVst>(hostcb),
      window_for_video(
          (glfwInit(), glfwSetErrorCallback(glfw_error_callback),
           glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true),
           glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4),
           glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6),
           glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE),
           glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE),
           glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE),
           glfwCreateWindow(100, 100, "", nullptr, nullptr))) {
  if (auto err = recompile_shaders()) {
    DBG << *err << '\n';
  }
}

OglerVst::~OglerVst() {
  // Need to make sure we bring the context back to the right thread before
  // destroying all of the OpenGL objects
  glfwMakeContextCurrent(window_for_video.get());
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
  std::unique_lock<std::mutex> lock(video_ctx_mtx);
  glfwMakeContextCurrent(window_for_video.get());

  auto video_prog = CreateProgram(data.video_shader);
  std::optional<std::string> res{};
  if (std::holds_alternative<std::string>(video_prog)) {
    res = std::make_optional<std::string>(
        std::move(std::get<std::string>(video_prog)));
  } else {
    prog = std::move(std::get<gl::Program>(video_prog));
  }
  glfwMakeContextCurrent(nullptr);

  return res;
}

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  std::unique_lock<std::mutex> lock(video_ctx_mtx);
  glfwMakeContextCurrent(window_for_video.get());
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(DebugCallback, nullptr);
  if (!prog) {
    return nullptr;
  }
  constexpr int out_w = 1024;
  constexpr int out_h = 768;
  if (!output_frame) {
    output_frame = std::unique_ptr<IVideoFrame>(
        new_video_frame(out_w, out_h, vst::FrameFormat::RGBA));
  }
  if (get_video_num_inputs() > 0 && !input_frame) {
    input_frame = std::unique_ptr<IVideoFrame>(
        get_video_input(0, vst::FrameFormat::RGBA));
    if (!inputTexture || inputTexture->get_width(0) != input_frame->get_w() ||
        inputTexture->get_height(0) != input_frame->get_h()) {
      inputTexture =
          gl::Texture2D::create(gl::InternalFormat::RGBA8, input_frame->get_w(),
                                input_frame->get_h());
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, ((input_frame->get_w() + 3) / 4) * 4);
    inputTexture->upload(0, 0, 0, input_frame->get_w(), input_frame->get_h(),
                         gl::PixelFormat::BGRA, gl::PixelType::UByte,
                         input_frame->get_bits());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    input_frame->Release();
  } else {
    input_frame = nullptr;
  }

  prog->use();
  if (auto resolution = prog->getUniform<gl::vec2>("iResolution")) {
    *resolution = gl::vec2{out_w, out_h};
  }
  if (auto time = prog->getUniform<float>("iTime")) {
    *time = project_time;
  }
  if (auto srate = prog->getUniform<float>("iSampleRate")) {
    *srate = host_get_sample_rate();
  }
  if (inputTexture) {
    if (auto cres = prog->getUniform<gl::vec2>("iChannelResolution")) {
      *cres = gl::vec2{float(inputTexture->get_width(0)),
                       float(inputTexture->get_height(0))};
    }
    if (auto ichannel = prog->getUniform<int>("iChannel")) {
      *ichannel = 0;
    }
    inputTexture->bindTextureUnit(0);
  }

  if (auto ochannel = prog->getUniform<int>("oChannel")) {
    *ochannel = 1;
  }

  if (!outputTexture) {
    outputTexture =
        gl::Texture2D::create(gl::InternalFormat::RGBA8, out_w, out_h);
  }
  outputTexture->bindImageUnit(1, 0, false, 0, gl::Access::WriteOnly,
                               gl::InternalFormat::RGBA8);

  glDispatchCompute(out_w, out_h, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);

  glPixelStorei(GL_PACK_ROW_LENGTH, ((output_frame->get_w() + 3) / 4) * 4);
  outputTexture->download(0, gl::PixelFormat::BGRA, gl::PixelType::UByte,
                          output_frame->get_rowspan() * output_frame->get_h(),
                          output_frame->get_bits());
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);

  // Let go of the context so that when the time comes to release objects, we do
  // not crash the main thread
  glfwMakeContextCurrent(nullptr);

  return output_frame.get();
}
} // namespace ogler