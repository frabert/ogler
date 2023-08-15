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

#include <istream>
#include <ostream>
#include <streambuf>

#include <clap/ext/state.h>

namespace clap {
template <typename Elem = char, typename Traits = std::char_traits<Elem>>
class istreambuf : public std::basic_streambuf<Elem, Traits> {
  const clap_istream_t *stream;

public:
  using char_type = Elem;
  using traits_type = Traits;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

  istreambuf(const clap_istream_t *stream) : stream(stream) {}

protected:
  int_type underflow() final { return uflow(); }

  int_type uflow() final {
    int_type value{};
    auto num_read = stream->read(stream, &value, 1);
    if (num_read < 0) {
      throw std::runtime_error("IO error");
    } else if (num_read == 0) {
      return traits_type::eof();
    } else {
      return value;
    }
  }
};

template <typename Elem = char, typename Traits = std::char_traits<Elem>>
class ostreambuf : public std::basic_streambuf<Elem, Traits> {
  const clap_ostream_t *stream;

public:
  using char_type = Elem;
  using traits_type = Traits;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

  ostreambuf(const clap_ostream_t *stream) : stream(stream) {}

protected:
  int_type overflow(int_type ch) final {
    if (ch != traits_type::eof()) {
      if (stream->write(stream, &ch, 1) == 1) {
        return ch;
      }
    }
    return traits_type::eof();
  }
};

template <typename T>
concept State = requires(T &plugin, std::istream &is, std::ostream &os) {
  { plugin.state_save(os) } -> std::same_as<bool>;
  { plugin.state_load(is) } -> std::same_as<bool>;
};

template <State Plugin> struct state {
  static constexpr const char *id = CLAP_EXT_STATE;

  template <typename Container> static const void *get_ext() {
    static clap_plugin_state_t state{
        .save =
            [](const clap_plugin_t *plugin, const clap_ostream_t *stream) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              ostreambuf ostreambuf(stream);
              std::ostream os(&ostreambuf);
              return self->plugin_data.state_save(os);
            },
        .load =
            [](const clap_plugin_t *plugin, const clap_istream_t *stream) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              istreambuf istreambuf(stream);
              std::istream is(&istreambuf);
              return self->plugin_data.state_load(is);
            },
    };
    return &state;
  }
};
} // namespace clap