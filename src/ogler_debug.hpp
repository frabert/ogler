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

#include <sstream>

#define DBG ::ogler::DebugStream()

namespace ogler {
template <typename Derived> class DebugStreamBase {
  std::stringstream stream;
  bool moved = false;

protected:
  DebugStreamBase(std::stringstream ss) : stream(std::move(ss)) {}

public:
  ~DebugStreamBase() noexcept(false) {
    if (!moved) {
      static_cast<Derived &>(*this).print(stream.str());
    }
  }

  template <typename V> Derived operator<<(V &&s) {
    moved = true;
    stream << std::forward<V>(s);
    return Derived(std::move(stream));
  }
};

class DebugStream : public DebugStreamBase<DebugStream> {
protected:
  friend class DebugStreamBase<DebugStream>;
  void print(const std::string &s);

public:
  DebugStream(std::stringstream ss = std::stringstream())
      : DebugStreamBase<DebugStream>(std::move(ss)) {}
};
} // namespace ogler