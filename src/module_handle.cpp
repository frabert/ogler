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

#include "module_handle.hpp"

#include "string_utils.hpp"

namespace ogler {
ModuleHandle::ModuleHandle(const std::filesystem::path &path) {
  auto str = OGLER_TO_WINSTR(path.string());
  hModule = LoadLibrary(str.c_str());

  if (hModule) {
    return;
  }

  auto err = GetLastError();
  std::error_code ec(err, std::system_category());
  throw std::system_error(ec, "Module loading failed");
}

ModuleHandle::~ModuleHandle() {
  if (!hModule) {
    return;
  }
  FreeModule(hModule);
}

FARPROC ModuleHandle::get_proc_addr(std::string_view proc) {
  return GetProcAddress(hModule, proc.data());
}
} // namespace ogler