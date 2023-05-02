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

HINSTANCE hInstance;

extern "C" {
int Scintilla_RegisterClasses(void *hInstance);
int Scintilla_ReleaseResources();
}

namespace ogler {
HINSTANCE get_hinstance() { return hInstance; }

std::unique_ptr<SharedVulkan> shared_vulkan;

SharedVulkan &OglerVst::get_shared_vulkan() { return *shared_vulkan; }

} // namespace ogler

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason,
                               LPVOID lpvReserved) {
  hInstance = hInst;
  if (dwReason == DLL_PROCESS_ATTACH) {
    glslang::InitializeProcess();
    if (!Scintilla_RegisterClasses(hInst)) {
      return false;
    }
    ogler::shared_vulkan = std::make_unique<ogler::SharedVulkan>();
  } else if (dwReason == DLL_PROCESS_DETACH) {
    glslang::FinalizeProcess();
    if (lpvReserved == NULL) {
      Scintilla_ReleaseResources();
    }
    ogler::shared_vulkan = nullptr;
  }
  return true;
}

extern "C" __declspec(dllexport) vst::AEffect *VSTPluginMain(
    vst::HostCallback *callback) {
  auto plugin = new ogler::OglerVst(callback);
  return plugin->get_effect();
}