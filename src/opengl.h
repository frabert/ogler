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

#include <gl.h>

#include <cassert>
#include <optional>
#include <string_view>
#include <variant>

namespace gl {
template <typename T> union tvec2 {
  struct {
    T x, y;
  };
  struct {
    T s, t;
  };
  struct {
    T r, g;
  };
};

template <typename T> union tvec3 {
  struct {
    T x, y, z;
  };
  struct {
    T s, t, p;
  };
  struct {
    T r, g, b;
  };
};

template <typename T> union tvec4 {
  struct {
    T x, y, z, w;
  };
  struct {
    T s, t, p, q;
  };
  struct {
    T r, g, b, a;
  };
};

using vec2 = tvec2<float>;
using vec3 = tvec3<float>;
using vec4 = tvec4<float>;

using dvec2 = tvec2<double>;
using dvec3 = tvec3<double>;
using dvec4 = tvec4<double>;

using ivec2 = tvec2<int>;
using ivec3 = tvec3<int>;
using ivec4 = tvec4<int>;

using uvec2 = tvec2<unsigned>;
using uvec3 = tvec3<unsigned>;
using uvec4 = tvec4<unsigned>;

enum class ShaderKind : GLenum {
  Fragment = GL_FRAGMENT_SHADER,
  Vertex = GL_VERTEX_SHADER,
  Compute = GL_COMPUTE_SHADER
};

class Shader final {
  friend class Program;
  GLuint id;

  Shader(GLuint id);

public:
  Shader(Shader &&);
  Shader(const Shader &) = delete;

  Shader &operator=(Shader &&);
  Shader &operator=(const Shader &) = delete;
  ~Shader();

  static std::variant<Shader, std::string> compile(ShaderKind type,
                                                   std::string_view source);
};

template <typename T> struct uniform_accessor;

template <> struct uniform_accessor<float> {
  static void get(GLuint prog, GLint loc, float &value) {
    glGetnUniformfv(prog, loc, 1, &value);
  }

  static void set(GLuint prog, GLint loc, float value) {
    glProgramUniform1f(prog, loc, value);
  }
};

template <> struct uniform_accessor<double> {
  static void get(GLuint prog, GLint loc, double &value) {
    glGetnUniformdv(prog, loc, 1, &value);
  }

  static void set(GLuint prog, GLint loc, double value) {
    glProgramUniform1d(prog, loc, value);
  }
};

template <> struct uniform_accessor<int> {
  static void get(GLuint prog, GLint loc, int &value) {
    glGetnUniformiv(prog, loc, 1, &value);
  }

  static void set(GLuint prog, GLint loc, int value) {
    glProgramUniform1i(prog, loc, value);
  }
};

template <> struct uniform_accessor<unsigned> {
  static void get(GLuint prog, GLint loc, unsigned &value) {
    glGetnUniformuiv(prog, loc, 1, &value);
  }

  static void set(GLuint prog, GLint loc, unsigned value) {
    glProgramUniform1ui(prog, loc, value);
  }
};

template <> struct uniform_accessor<vec2> {
  static void get(GLuint prog, GLint loc, vec2 &value) {
    glGetnUniformfv(prog, loc, 2, &value.x);
  }

  static void set(GLuint prog, GLint loc, vec2 value) {
    glProgramUniform2f(prog, loc, value.x, value.y);
  }
};

template <> struct uniform_accessor<dvec2> {
  static void get(GLuint prog, GLint loc, dvec2 &value) {
    glGetnUniformdv(prog, loc, 2, &value.x);
  }

  static void set(GLuint prog, GLint loc, dvec2 value) {
    glProgramUniform2d(prog, loc, value.x, value.y);
  }
};

template <> struct uniform_accessor<ivec2> {
  static void get(GLuint prog, GLint loc, ivec2 &value) {
    glGetnUniformiv(prog, loc, 2, &value.x);
  }

  static void set(GLuint prog, GLint loc, ivec2 value) {
    glProgramUniform2i(prog, loc, value.x, value.y);
  }
};

template <> struct uniform_accessor<uvec2> {
  static void get(GLuint prog, GLint loc, uvec2 &value) {
    glGetnUniformuiv(prog, loc, 2, &value.x);
  }

  static void set(GLuint prog, GLint loc, uvec2 value) {
    glProgramUniform2ui(prog, loc, value.x, value.y);
  }
};

template <> struct uniform_accessor<vec3> {
  static void get(GLuint prog, GLint loc, vec3 &value) {
    glGetnUniformfv(prog, loc, 3, &value.x);
  }

  static void set(GLuint prog, GLint loc, vec3 value) {
    glProgramUniform3f(prog, loc, value.x, value.y, value.z);
  }
};

template <> struct uniform_accessor<dvec3> {
  static void get(GLuint prog, GLint loc, dvec3 &value) {
    glGetnUniformdv(prog, loc, 3, &value.x);
  }

  static void set(GLuint prog, GLint loc, dvec3 value) {
    glProgramUniform3d(prog, loc, value.x, value.y, value.z);
  }
};

template <> struct uniform_accessor<ivec3> {
  static void get(GLuint prog, GLint loc, ivec3 &value) {
    glGetnUniformiv(prog, loc, 3, &value.x);
  }

  static void set(GLuint prog, GLint loc, ivec3 value) {
    glProgramUniform3i(prog, loc, value.x, value.y, value.z);
  }
};

template <> struct uniform_accessor<uvec3> {
  static void get(GLuint prog, GLint loc, uvec3 &value) {
    glGetnUniformuiv(prog, loc, 3, &value.x);
  }

  static void set(GLuint prog, GLint loc, uvec3 value) {
    glProgramUniform3ui(prog, loc, value.x, value.y, value.z);
  }
};

template <> struct uniform_accessor<vec4> {
  static void get(GLuint prog, GLint loc, vec4 &value) {
    glGetnUniformfv(prog, loc, 4, &value.x);
  }

  static void set(GLuint prog, GLint loc, vec4 value) {
    glProgramUniform4f(prog, loc, value.x, value.y, value.z, value.w);
  }
};

template <> struct uniform_accessor<dvec4> {
  static void get(GLuint prog, GLint loc, dvec4 &value) {
    glGetnUniformdv(prog, loc, 4, &value.x);
  }

  static void set(GLuint prog, GLint loc, dvec4 value) {
    glProgramUniform4d(prog, loc, value.x, value.y, value.z, value.w);
  }
};

template <> struct uniform_accessor<ivec4> {
  static void get(GLuint prog, GLint loc, ivec4 &value) {
    glGetnUniformiv(prog, loc, 4, &value.x);
  }

  static void set(GLuint prog, GLint loc, ivec4 value) {
    glProgramUniform4i(prog, loc, value.x, value.y, value.z, value.w);
  }
};

template <> struct uniform_accessor<uvec4> {
  static void get(GLuint prog, GLint loc, uvec4 &value) {
    glGetnUniformuiv(prog, loc, 4, &value.x);
  }

  static void set(GLuint prog, GLint loc, uvec4 value) {
    glProgramUniform4ui(prog, loc, value.x, value.y, value.z, value.w);
  }
};

template <typename T> class Uniform final {
  friend class Program;

  GLuint prog;
  GLint loc;

  Uniform(GLuint prog, GLint loc) : prog(prog), loc(loc) {}

public:
  Uniform(Uniform &&u) {
    prog = u.prog;
    loc = u.loc;
  }

  Uniform(const Uniform &u) = delete;

  Uniform &operator=(Uniform &&u) {
    prog = u.prog;
    loc = u.loc;

    return *this;
  }

  Uniform &operator=(const Uniform &u) = delete;

  operator T() const {
    T res;
    uniform_accessor<T>::get(prog, loc, res);
    return res;
  }

  const Uniform &operator=(const T &value) const {
    uniform_accessor<T>::set(prog, loc, value);

    return *this;
  }
};

class Program final {
  GLuint id;

  Program(GLuint id);

public:
  Program(Program &&);
  Program(const Program &) = delete;

  Program &operator=(Program &&);
  Program &operator=(const Program &) = delete;
  ~Program();

  void use() const;

  template <typename... Shaders>
  static std::variant<Program, std::string> link(const Shaders &...shaders) {
    auto id = glCreateProgram();
    if (!id) {
      return "Cannot create program";
    }

    (glAttachShader(id, shaders.id), ...);
    glLinkProgram(id);

    int success;
    glGetProgramiv(id, GL_LINK_STATUS, &success);
    if (!success) {
      int log_size;
      glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_size);
      std::string str;
      str.reserve(log_size - 1);
      glGetProgramInfoLog(id, str.capacity(), nullptr, str.data());
      glDeleteProgram(id);
      return str;
    }

    return Program{id};
  }

  template <typename T>
  std::optional<Uniform<T>> getUniform(const std::string &name) const {
    auto loc = glGetUniformLocation(id, name.c_str());
    if (loc < 0) {
      return std::nullopt;
    }
    return Uniform<T>(id, loc);
  }
};

enum class InternalFormat : GLenum {
  R8 = GL_R8,
  RG8 = GL_RG8,
  RGB8 = GL_RGB8,
  RGBA8 = GL_RGBA8,

  R16F = GL_R16F,
  RG16F = GL_RG16F,
  RGB16F = GL_RGB16F,
  RGBA16F = GL_RGBA16F,

  R32F = GL_R32F,
  RG32F = GL_RG32F,
  RGB32F = GL_RGB32F,
  RGBA32F = GL_RGBA32F,

  DepthComponent16 = GL_DEPTH_COMPONENT16,
  DepthComponent24 = GL_DEPTH_COMPONENT24,
  DepthComponent32F = GL_DEPTH_COMPONENT32F,

  Depth32FStencil8 = GL_DEPTH32F_STENCIL8,
  Depth24Stencil8 = GL_DEPTH24_STENCIL8,
  StencilIndex8 = GL_STENCIL_INDEX8
};

class Buffer final {
  friend class VertexArray;

  GLuint id;

  Buffer(GLuint id);

public:
  ~Buffer();

  Buffer(Buffer &&b);
  Buffer(const Buffer &&) = delete;
  Buffer &operator=(Buffer &&b);
  Buffer &operator=(const Buffer &) = delete;

  void write(size_t offset, size_t size, const void *data) const;
  void copyTo(const Buffer &target, size_t source_offset, size_t target_offset,
              size_t size) const;
  void read(size_t offset, size_t size, void *target);
  void invalidate() const;
  void invalidate(size_t offset, size_t size) const;
  void clear(GLenum internalformat, GLenum format, GLenum type,
             const void *data) const;

  void bind(GLenum target) const;
  void bind(GLenum target, GLuint index) const;

  static std::optional<Buffer> create(size_t size, GLbitfield flags,
                                      const void *data = nullptr);
};

class VertexArray final {
  GLuint id;

public:
  VertexArray();
  ~VertexArray();

  VertexArray(VertexArray &&b);
  VertexArray(const VertexArray &&) = delete;
  VertexArray &operator=(VertexArray &&b);
  VertexArray &operator=(const VertexArray &) = delete;

  void vertexBuffer(const Buffer &buffer, int binding, size_t offset,
                    size_t stride) const;

  void indexBuffer(const Buffer &buffer) const;
  void enableAttrib(int index) const;
  void attribFormat(int index, size_t size, GLenum type, bool normalized,
                    size_t offset) const;
  void attribBinding(int index, int binding) const;

  void bind() const;
};

enum class PixelFormat : GLenum {
  R = GL_RED,
  RG = GL_RG,
  RGB = GL_RGB,
  BGR = GL_BGR,
  RGBA = GL_RGBA,
  BGRA = GL_BGRA,
  DepthComponent = GL_DEPTH_COMPONENT,
  StencilIndex = GL_STENCIL_INDEX
};

enum class PixelType : GLenum {
  UByte = GL_UNSIGNED_BYTE,
  Byte = GL_BYTE,
  UShort = GL_UNSIGNED_SHORT,
  Short = GL_SHORT,
  UInt = GL_UNSIGNED_INT,
  Int = GL_INT,
  Float = GL_FLOAT,
  UByte332 = GL_UNSIGNED_BYTE_3_3_2,
  UByte233 = GL_UNSIGNED_BYTE_2_3_3_REV,
  UShort565 = GL_UNSIGNED_SHORT_5_6_5,
  UShort565Rev = GL_UNSIGNED_SHORT_5_6_5_REV,
  UShort4444 = GL_UNSIGNED_SHORT_4_4_4_4,
  UShort4444Rev = GL_UNSIGNED_SHORT_4_4_4_4_REV,
  UShort5551 = GL_UNSIGNED_SHORT_5_5_5_1,
  UShort1555 = GL_UNSIGNED_SHORT_1_5_5_5_REV,
  UInt8888 = GL_UNSIGNED_INT_8_8_8_8,
  UInt8888Rev = GL_UNSIGNED_INT_8_8_8_8_REV,
  UInt1010102 = GL_UNSIGNED_INT_10_10_10_2,
  UInt2101010 = GL_UNSIGNED_INT_2_10_10_10_REV
};

enum class Access : GLenum {
  ReadOnly = GL_READ_ONLY,
  WriteOnly = GL_WRITE_ONLY,
  ReadWrite = GL_READ_WRITE
};

class Texture2D final {
  GLuint id;

  Texture2D(GLuint id);

public:
  ~Texture2D();

  Texture2D(Texture2D &&b);
  Texture2D(const Texture2D &&) = delete;
  Texture2D &operator=(Texture2D &&b);
  Texture2D &operator=(const Texture2D &) = delete;

  void upload(GLint level, GLint xoffset, GLint yoffset, GLsizei width,
              GLsizei height, PixelFormat format, PixelType type,
              const void *data);
  void download(GLint level, PixelFormat format, PixelType type,
                GLsizei bufSize, void *pixels);

  void bindTextureUnit(GLuint unit);
  void bindImageUnit(GLuint unit, GLint level, bool layered, GLint layer,
                     Access access, InternalFormat format);

  GLint get_width(GLint level);
  GLint get_height(GLint level);

  static std::optional<Texture2D> create(InternalFormat internalformat,
                                         GLsizei width, GLsizei height);
};
} // namespace gl