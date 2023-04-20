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

#include "opengl.h"
#include "gl.h"

#include <cassert>
#include <initializer_list>
#include <optional>

namespace gl {
Shader::Shader(GLuint id) : id(id) { assert(glIsShader(id)); }
Shader::Shader(Shader &&s) : id(s.id) { s.id = 0; }

Shader &Shader::operator=(Shader &&s) {
  id = s.id;
  s.id = 0;

  return *this;
}

Shader::~Shader() {
  if (id) {
    glDeleteShader(id);
  }
}

std::variant<Shader, std::string>
Shader::compile(ShaderKind type, const std::vector<std::string_view> &sources) {
  auto id = glCreateShader(static_cast<GLenum>(type));
  if (!id) {
    return "Cannot create shader";
  }

  std::vector<GLint> sizes(sources.size());
  std::vector<const char *> data(sources.size());
  for (size_t i = 0; i < sources.size(); ++i) {
    sizes[i] = sources[i].size();
    data[i] = sources[i].data();
  }

  glShaderSource(id, sources.size(), data.data(), sizes.data());
  glCompileShader(id);

  int success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);
  if (!success) {
    int log_size;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_size);
    std::string str;
    str.reserve(log_size - 1);
    glGetShaderInfoLog(id, str.capacity(), nullptr, str.data());
    glDeleteShader(id);
    return std::move(str);
  }

  return Shader{id};
}

Program::Program(GLuint id) : id(id) { assert(glIsProgram(id)); }
Program::Program(Program &&s) : id(s.id) { s.id = 0; }

Program &Program::operator=(Program &&s) {
  id = s.id;
  s.id = 0;

  return *this;
}

Program::~Program() {
  if (id) {
    glDeleteProgram(id);
  }
}

void Program::use() const { glUseProgram(id); }

Buffer::Buffer(GLuint id) : id(id) { assert(glIsBuffer(id)); }

Buffer::~Buffer() {
  if (id) {
    glDeleteBuffers(1, &id);
  }
}

Buffer::Buffer(Buffer &&b) : id(b.id) { b.id = 0; }

Buffer &Buffer::operator=(Buffer &&b) {
  id = b.id;
  b.id = 0;

  return *this;
}

void Buffer::write(size_t offset, size_t size, const void *data) const {
  glNamedBufferSubData(id, offset, size, data);
}

void Buffer::copyTo(const Buffer &target, size_t source_offset,
                    size_t target_offset, size_t size) const {
  glCopyNamedBufferSubData(id, target.id, source_offset, target_offset, size);
}

void Buffer::read(size_t offset, size_t size, void *data) {
  glGetNamedBufferSubData(id, offset, size, data);
}

void Buffer::invalidate() const { glInvalidateBufferData(id); }

void Buffer::invalidate(size_t offset, size_t size) const {
  glInvalidateBufferSubData(id, offset, size);
}

void Buffer::clear(GLenum internalformat, GLenum format, GLenum type,
                   const void *data) const {
  glClearNamedBufferData(id, internalformat, format, type, data);
}

void Buffer::bind(GLenum target) const { glBindBuffer(target, id); }

void Buffer::bind(GLenum target, GLuint index) const {
  glBindBufferBase(target, index, id);
}

std::optional<Buffer> Buffer::create(size_t size, GLbitfield flags,
                                     const void *data) {
  GLuint id;
  glCreateBuffers(1, &id);
  if (!id) {
    return std::nullopt;
  }

  glNamedBufferStorage(id, size, data, flags);
  return Buffer(id);
}

VertexArray::VertexArray() {
  glCreateVertexArrays(1, &id);
  assert(glIsVertexArray(id));
}

VertexArray::~VertexArray() {
  if (id) {
    glDeleteVertexArrays(1, &id);
  }
}

VertexArray::VertexArray(VertexArray &&b) : id(b.id) { b.id = 0; }

VertexArray &VertexArray::operator=(VertexArray &&b) {
  id = b.id;
  b.id = 0;

  return *this;
}

void VertexArray::vertexBuffer(const Buffer &buffer, int index, size_t offset,
                               size_t stride) const {
  glVertexArrayVertexBuffer(id, index, buffer.id, offset, stride);
}

void VertexArray::indexBuffer(const Buffer &buffer) const {
  glVertexArrayElementBuffer(id, buffer.id);
}

void VertexArray::enableAttrib(int index) const {
  glEnableVertexArrayAttrib(id, index);
}

void VertexArray::attribFormat(int index, size_t size, GLenum type,
                               bool normalized, size_t offset) const {
  glVertexArrayAttribFormat(id, index, size, type, normalized, offset);
}

void VertexArray::attribBinding(int index, int binding) const {
  glVertexArrayAttribBinding(id, index, binding);
}

void VertexArray::bind() const { glBindVertexArray(id); }

Texture2D::Texture2D(GLuint id) : id(id) { assert(glIsTexture(id)); }
Texture2D::~Texture2D() {
  if (id) {
    glDeleteTextures(1, &id);
  }
}

Texture2D::Texture2D(Texture2D &&b) : id(b.id) { b.id = 0; }
Texture2D &Texture2D::operator=(Texture2D &&b) {
  id = b.id;
  b.id = 0;
  return *this;
}

void Texture2D::upload(GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                       GLsizei height, PixelFormat format, PixelType type,
                       const void *data) {
  glTextureSubImage2D(id, level, xoffset, yoffset, width, height,
                      static_cast<GLenum>(format), static_cast<GLenum>(type),
                      data);
}

void Texture2D::download(GLint level, PixelFormat format, PixelType type,
                         GLsizei bufSize, void *pixels) {
  glGetTextureImage(id, level, static_cast<GLenum>(format),
                    static_cast<GLenum>(type), bufSize, pixels);
}

void Texture2D::bindTextureUnit(GLuint unit) { glBindTextureUnit(unit, id); }

void Texture2D::bindImageUnit(GLuint unit, GLint level, bool layered,
                              GLint layer, Access access,
                              InternalFormat format) {
  glBindImageTexture(unit, id, level, layered ? GL_TRUE : GL_FALSE, layer,
                     static_cast<GLenum>(access), static_cast<GLenum>(format));
}

GLint Texture2D::get_width(GLint level) {
  GLint value;
  glGetTextureLevelParameteriv(id, level, GL_TEXTURE_WIDTH, &value);
  return value;
}

GLint Texture2D::get_height(GLint level) {
  GLint value;
  glGetTextureLevelParameteriv(id, level, GL_TEXTURE_HEIGHT, &value);
  return value;
}

std::optional<Texture2D> Texture2D::create(InternalFormat internalformat,
                                           GLsizei width, GLsizei height) {
  GLuint id;
  glCreateTextures(GL_TEXTURE_2D, 1, &id);
  glTextureStorage2D(id, 1, static_cast<GLenum>(internalformat), width, height);

  return Texture2D{id};
}

} // namespace gl