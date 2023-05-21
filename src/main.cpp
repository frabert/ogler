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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>

#include <glslang/Public/ShaderLang.h>

#include "clap/plugin.hpp"

HINSTANCE hInstance;

extern "C" {
int Scintilla_RegisterClasses(void *hInstance);
int Scintilla_ReleaseResources();
}

namespace ogler {
HINSTANCE get_hinstance() { return hInstance; }

SharedVulkan &Ogler::get_shared_vulkan() {
  static SharedVulkan shared_vulkan;
  return shared_vulkan;
}

} // namespace ogler

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason,
                               LPVOID lpvReserved) {
  hInstance = hInst;
  if (dwReason == DLL_PROCESS_ATTACH) {
    glslang::InitializeProcess();
    if (!Scintilla_RegisterClasses(hInst)) {
      return false;
    }
  } else if (dwReason == DLL_PROCESS_DETACH) {
    glslang::FinalizeProcess();
    if (lpvReserved == NULL) {
      Scintilla_ReleaseResources();
    }
  }
  return true;
}

static_assert(clap::SupportsState<ogler::Ogler>);
static_assert(clap::SupportsGui<ogler::Ogler>);
static_assert(clap::SupportsParams<ogler::Ogler>);

CLAP_EXPORT extern "C" const clap_plugin_entry_t clap_entry{
    .clap_version = CLAP_VERSION,
    .init = [](const char *plugin_path) { return true; },
    .deinit = []() {},
    .get_factory = [](const char *factory_id) -> const void * {
      if (std::string_view{factory_id} != CLAP_PLUGIN_FACTORY_ID) {
        return nullptr;
      }

      return &clap::plugin_factory<ogler::Ogler>::value;
    },
};