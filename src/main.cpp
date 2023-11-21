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

#include "ogler.hpp"

#include <glslang/Public/ShaderLang.h>

#include "clap/ext/audio-ports.hpp"
#include "clap/ext/gui.hpp"
#include "clap/ext/params.hpp"
#include "clap/ext/state.hpp"
#include "clap/plugin.hpp"

#include <sstream>

#include "sciter_scintilla.hpp"
#include "string_utils.hpp"

HINSTANCE hInstance;

extern "C" {
int Scintilla_RegisterClasses(void *hInstance);
int Scintilla_ReleaseResources();
}

namespace ogler {
HINSTANCE get_hinstance() { return hInstance; }

static std::unique_ptr<SharedVulkan> shared_vulkan = nullptr;
static std::unique_ptr<ogler::ScintillaEditorFactory> scintilla_factory =
    nullptr;

SharedVulkan &Ogler::get_shared_vulkan() { return *shared_vulkan; }

} // namespace ogler

extern "C" BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason,
                               LPVOID lpvReserved) {
  hInstance = hInst;
  if (dwReason == DLL_PROCESS_ATTACH) {
    if (!Scintilla_RegisterClasses(hInst)) {
      return false;
    }
  } else if (dwReason == DLL_PROCESS_DETACH) {
    if (lpvReserved == NULL) {
      Scintilla_ReleaseResources();
    }
  }
  return true;
}

using ogler_plugin = clap::plugin<ogler::Ogler, clap::state, clap::gui,
                                  clap::params, clap::audio_ports>;

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry{
    .clap_version = CLAP_VERSION,
    .init =
        [](const char *plugin_path) {
          glslang::InitializeProcess();
          try {
            ogler::shared_vulkan = std::make_unique<ogler::SharedVulkan>();
          } catch (vk::Error &err) {
            std::stringstream errmsg;
            errmsg << "ogler could not initialize the Vulkan context:\n\n"
                   << err.what();
            auto msg = OGLER_TO_WINSTR(errmsg.str());

            MessageBox(nullptr, msg.c_str(), TEXT("ogler initialization error"),
                       MB_ICONERROR | MB_OK);
            return false;
          }

          ogler::scintilla_factory =
              std::make_unique<ogler::ScintillaEditorFactory>(
                  ogler::get_hinstance());
          return true;
        },
    .deinit =
        []() {
          glslang::FinalizeProcess();
          ogler::shared_vulkan = nullptr;
          ogler::scintilla_factory = nullptr;
        },
    .get_factory = &clap::plugin_factory<ogler_plugin>::getter,
};