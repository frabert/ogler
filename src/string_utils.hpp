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

#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace ogler {
std::string to_string(const std::wstring &wstring);
inline std::string to_string(const std::string &string) { return string; }
std::wstring to_wstring(const std::string &string);
inline std::wstring to_wstring(const std::wstring &wstring) { return wstring; }

#ifdef UNICODE
using WinStr = LPCWSTR;
using WinStrView = std::wstring_view;
#define OGLER_TO_WINSTR(x) ::ogler::to_wstring((x))
#else
using WinStr = LPCSTR;
using WinStrView = std::string_view;
#define OGLER_TO_WINSTR(x) ::ogler::to_string((x))
#endif
} // namespace ogler