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
*/

#include "ogler_debug.hpp"

#include <reaper_plugin_functions.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace ogler {
void DebugStream::print(const std::string &s) { OutputDebugString(s.c_str()); }

void ReaperStream::print(const std::string &s) {
  if (!ShowConsoleMsg) {
    return;
  }
  ShowConsoleMsg(s.c_str());
}
} // namespace ogler