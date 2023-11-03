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

#include "IReaper.h"
#include "clap/ext/log.h"
#include "clap/host.hpp"

#include <WDL/eel2/ns-eel.h>

#include <reaper_plugin.h>

#include <cassert>
#include <mutex>

namespace ogler {

class RealReaper final : public IReaper {
  const clap::host &host;

  template <typename T>
  void get_reaper_function(std::string_view name, T &out) {
    auto reaper_plugin =
        host.get_extension<reaper_plugin_info_t>("cockos.reaper_extension");
    out = reinterpret_cast<T>(reaper_plugin->GetFunc(name.data()));
  }

  int reaper_vidw_idx;
  int reaper_vidh_idx;

  void (*enter_mutex)();
  void (*leave_mutex)();

  double ***(*eel_gmem_attach_f)(const char *name, bool is_alloc);
  void *(*clap_get_reaper_context)(const clap_host_t *host, int request);
  IREAPERVideoProcessor *(*video_CreateVideoProcessor)(void *fxctx,
                                                       int version);
  void (*ShowConsoleMsg)(const char *msg);
  ReaProject *(*EnumProjects)(int idx, char *projfnOutOptional,
                              int projfnOutOptional_sz);

  void *(*projectconfig_var_addr)(ReaProject *proj, int idx);
  int (*projectconfig_var_getoffs)(const char *name, int *szOut);

public:
  RealReaper(const clap::host &host) : host(host) {
    get_reaper_function("NSEEL_HOSTSTUB_EnterMutex", enter_mutex);
    get_reaper_function("NSEEL_HOSTSTUB_LEAVEMutex", leave_mutex);
    get_reaper_function("eel_gmem_attach", eel_gmem_attach_f);
    get_reaper_function("video_CreateVideoProcessor",
                        video_CreateVideoProcessor);
    get_reaper_function("ShowConsoleMsg", ShowConsoleMsg);
    get_reaper_function("EnumProjects", EnumProjects);
    get_reaper_function("clap_get_reaper_context", clap_get_reaper_context);

    get_reaper_function("projectconfig_var_addr", projectconfig_var_addr);
    get_reaper_function("projectconfig_var_getoffs", projectconfig_var_getoffs);

    int sz{};
    reaper_vidw_idx = projectconfig_var_getoffs("projvidw", &sz);
    assert(sz == 4);

    reaper_vidh_idx = projectconfig_var_getoffs("projvidh", &sz);
    assert(sz == 4);
  }

  EELMutex get_eel_mutex() { return EELMutex(enter_mutex, leave_mutex); }

  double ***eel_gmem_attach() { return eel_gmem_attach_f("ogler", true); }

  std::unique_ptr<IREAPERVideoProcessor> create_video_processor() {
    auto reaper_ctx = clap_get_reaper_context(&host, 4);

    return std::unique_ptr<IREAPERVideoProcessor>(video_CreateVideoProcessor(
        reaper_ctx, IREAPERVideoProcessor::REAPER_VIDEO_PROCESSOR_VERSION));
  }

  std::pair<int, int> get_current_project_size(int fallback_width,
                                               int fallback_height) {
    auto cur_proj = EnumProjects(-1, nullptr, 0);
    if (!cur_proj) {
      return {fallback_width, fallback_height};
    }

    auto project_output_width =
        *static_cast<int *>(projectconfig_var_addr(cur_proj, reaper_vidw_idx));
    auto project_output_height =
        *static_cast<int *>(projectconfig_var_addr(cur_proj, reaper_vidh_idx));

    if (!project_output_width) {
      project_output_width = fallback_width;
    }

    if (!project_output_height) {
      project_output_height = fallback_height;
    }

    return {project_output_width, project_output_height};
  }

  void print_console(const char *msg) { ShowConsoleMsg(msg); }
};

static std::mutex mtx;

using gmem_block = double[NSEEL_RAM_ITEMSPERBLOCK];
static gmem_block gmem[NSEEL_RAM_BLOCKS];

class MockVideoProcessor final : public IREAPERVideoProcessor {
public:
  IVideoFrame *newVideoFrame(int w, int h, int fmt) { return nullptr; }

  int getNumInputs() { return 0; }
  int getInputInfo(int idx, void **itemptr) { return 0; }
  IVideoFrame *renderInputVideoFrame(int idx, int want_fmt /*0 for native*/) {
    return nullptr;
  }
};

class MockReaper final : public IReaper {
  const clap::host &host;

public:
  MockReaper(const clap::host &host) : host(host) {}

  EELMutex get_eel_mutex() {
    return EELMutex([]() { mtx.lock(); }, []() { mtx.unlock(); });
  }

  double ***eel_gmem_attach() { return reinterpret_cast<double ***>(&gmem); }

  std::unique_ptr<IREAPERVideoProcessor> create_video_processor() {
    return std::make_unique<MockVideoProcessor>();
  }

  std::pair<int, int> get_current_project_size(int fallback_width,
                                               int fallback_height) {
    return {128, 128};
  }

  void print_console(const char *msg) { host.log(CLAP_LOG_INFO, msg); }
};

std::unique_ptr<IReaper> IReaper::get_reaper(const clap::host &host) {
  if (std::string_view{host.vendor} != "Cockos" ||
      std::string_view{host.name} != "REAPER") {
    return std::make_unique<MockReaper>(host);
  }

  return std::make_unique<RealReaper>(host);
}
} // namespace ogler