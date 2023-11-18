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

#pragma once

#include <concepts>

#include <clap/ext/state.h>

namespace clap {
class istream : public clap_istream_t {
public:
  inline int64_t read(void *buffer, uint64_t size) const {
    return static_cast<const clap_istream_t *>(this)->read(this, buffer, size);
  }
};
static_assert(sizeof(istream) == sizeof(clap_istream_t));

class ostream : public clap_ostream_t {
public:
  inline int64_t write(const void *buffer, uint64_t size) const {
    return static_cast<const clap_ostream_t *>(this)->write(this, buffer, size);
  }
};
static_assert(sizeof(ostream) == sizeof(clap_ostream_t));

template <typename T>
concept State = requires(T &plugin, const istream &is, const ostream &os) {
  { plugin.state_save(os) } -> std::same_as<bool>;
  { plugin.state_load(is) } -> std::same_as<bool>;
};

template <State Plugin> struct state {
  static constexpr const char *id = CLAP_EXT_STATE;

  template <typename Container> struct impl {
    static const void *get() {
      static clap_plugin_state_t state{
          .save =
              [](const clap_plugin_t *plugin, const clap_ostream_t *stream) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                auto os = static_cast<const ostream *>(stream);
                return self->plugin_data.state_save(*os);
              },
          .load =
              [](const clap_plugin_t *plugin, const clap_istream_t *stream) {
                auto self = static_cast<Container *>(plugin->plugin_data);
                auto is = static_cast<const istream *>(stream);
                return self->plugin_data.state_load(*is);
              },
      };
      return &state;
    }
  };
};
} // namespace clap