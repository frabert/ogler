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

#include "opengl.h"

#include <GLFW/glfw3.h>
#include <gl.h>
#include <reaper_plugin_functions.h>

#include <optional>
#include <sstream>
#include <utility>
#include <variant>

void std::default_delete<GLFWwindow>::operator()(GLFWwindow *wnd) const {
  glfwDestroyWindow(wnd);
}

namespace ogler {

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

#define DBG ReaperStream()

static void DebugCallback(GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei length,
                          const GLchar *message, const void *userParam) {
  DBG << "GL error: " << message << '\n';
}

static std::optional<gl::Program> CreateProgram() {
  auto shader = gl::Shader::compile(gl::ShaderKind::Compute, R"(
#version 440

layout(local_size_x = 1, local_size_y = 1) in;

uniform vec2 iResolution;
uniform float iTime;
uniform float iSampleRate;
uniform vec2 iChannelResolution;
uniform sampler2D iChannel;
uniform writeonly image2D oChannel;

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}

void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID.xy));
    imageStore(oChannel, ivec2(gl_GlobalInvocationID.xy), fragColor);
})");

  if (!std::holds_alternative<gl::Shader>(shader)) {
    DBG << std::get<std::string>(shader) << '\n';
    return std::nullopt;
  }
  auto prog = gl::Program::link(std::get<gl::Shader>(shader));

  if (!std::holds_alternative<gl::Program>(prog)) {
    DBG << std::get<std::string>(prog) << '\n';
    return std::nullopt;
  }
  return std::move(std::get<gl::Program>(prog));
}

OglerVst::OglerVst(vst::HostCallback *hostcb)
    : vst::ReaperVstPlugin<OglerVst>(hostcb),
      window((glfwInit(), glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true),
              glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4),
              glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6),
              glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE),
              glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE),
              glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE),
              glfwCreateWindow(100, 100, "", nullptr, nullptr))) {}

OglerVst::~OglerVst() {
  // Need to make sure we bring the context back to the right thread before
  // destroying all of the OpenGL objects
  glfwMakeContextCurrent(window.get());
}

std::string_view OglerVst::get_effect_name() noexcept { return "Ogler"; }
std::string_view OglerVst::get_vendor_name() noexcept {
  return "Francesco Bertolaccini";
}
std::string_view OglerVst::get_product_name() noexcept { return "Ogler"; }
std::int32_t OglerVst::get_vendor_version() noexcept { return 1000; }

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  glfwMakeContextCurrent(window.get());
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(DebugCallback, nullptr);
  if (!prog) {
    prog = CreateProgram();
  }
  constexpr int out_w = 1920;
  constexpr int out_h = 1080;
  auto glrc = wglGetCurrentContext();
  if (!frame) {
    frame = new_video_frame(out_w, out_h, vst::FrameFormat::RGBA);
  }
  if (get_video_num_inputs() > 0) {
    auto input = get_video_input(0, vst::FrameFormat::RGBA);
    if (!inputTexture || inputTexture->get_width(0) != input->get_w() ||
        inputTexture->get_height(0) != input->get_h()) {
      inputTexture = gl::Texture2D::create(gl::InternalFormat::RGBA8,
                                           input->get_w(), input->get_h());
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, input->get_rowspan());
    inputTexture->upload(0, 0, 0, input->get_w(), input->get_h(),
                         gl::PixelFormat::RGBA, gl::PixelType::UByte,
                         input->get_bits());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    input->Release();
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
    if (auto ichannel = prog->getUniform<unsigned>("iChannel")) {
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

  glPixelStorei(GL_UNPACK_ROW_LENGTH, frame->get_rowspan());
  outputTexture->download(0, gl::PixelFormat::RGBA, gl::PixelType::UByte,
                          frame->get_rowspan() * frame->get_h(),
                          frame->get_bits());
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  // Let go of the context so that when the time comes to release objects, we do
  // not crash the main thread
  glfwMakeContextCurrent(nullptr);

  return frame;
}
} // namespace ogler