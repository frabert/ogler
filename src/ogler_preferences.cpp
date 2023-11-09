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

#include <array>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ogler_preferences.hpp"

namespace {
std::string ReadString(std::string_view key, std::string_view def,
                       std::string_view file) {
  std::array<char, 255> buf;
  int size = GetPrivateProfileString("ogler", key.data(), def.data(),
                                     buf.data(), buf.size(), file.data());
  return std::string(buf.data(), size);
}

int ReadInt(std::string_view key, int def, std::string_view file) {
  auto res = ReadString(key, "", file);
  if (res.empty()) {
    return def;
  } else {
    return std::stoi(res);
  }
}

void WriteString(std::string_view key, std::string_view value,
                 std::string_view file) {
  WritePrivateProfileString("ogler", key.data(), value.data(), file.data());
}

void WriteInt(std::string_view key, int value, std::string_view file) {
  WriteString(key, std::to_string(value), file);
}
} // namespace

namespace ogler {
Preferences::Preferences(const char *file) : file(file) {}

std::string Preferences::get_font_face() const {
  return ReadString("font_face", "Courier New", file);
}
bool Preferences::set_font_face(const std::string &name) {
  WriteString("font_face", name, file);
  return true;
}

int Preferences::get_font_size() const {
  return ReadInt("font_size", 12, file);
}
bool Preferences::set_font_size(int size) {
  WriteInt("font_size", size, file);
  return true;
}

int Preferences::get_view_ws() const { return ReadInt("view_ws", 0, file); }
bool Preferences::set_view_ws(int value) {
  WriteInt("view_ws", value, file);
  return true;
}

bool Preferences::get_use_tabs() const { return ReadInt("use_tabs", 0, file); }
bool Preferences::set_use_tabs(bool value) {
  WriteInt("use_tabs", value, file);
  return true;
}

int Preferences::get_tab_width() const { return ReadInt("tab_width", 4, file); }
bool Preferences::set_tab_width(int value) {
  WriteInt("tab_width", value, file);
  return true;
}

PreferencesWindow::PreferencesWindow(HWND hWnd, HINSTANCE hinstance,
                                     HMENU hMenu, HWND hwndParent, int cy,
                                     int cx, int y, int x, LONG style,
                                     const char *name, const char *cls,
                                     DWORD exStyle, const char *file)
    : hwnd(hWnd), file(file) {}

void PreferencesWindow::window_created() {
  auto value = sciter::value::wrap_asset(new Preferences(file));
  SciterSetVariable(hwnd, "ogler_preferences", &value);
  SciterLoadFile(hwnd, L"res://ui/settings.html");
}

PreferencesWindow::~PreferencesWindow() {
  sciter::value val = {};
  SciterSetVariable(hwnd, "ogler_preferences", &val);
}
} // namespace ogler