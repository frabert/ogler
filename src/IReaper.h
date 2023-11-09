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

#include <WDL/wdltypes.h>

#include <video_frame.h>
#include <video_processor.h>

#include <memory>
#include <utility>

#include "clap/host.hpp"

namespace ogler {

using eel_gmem_attach_f = double ***(const char *name, bool is_alloc);
using mutex_stub_f = void();

class EELMutex {
  mutex_stub_f *enter;
  mutex_stub_f *leave;

public:
  EELMutex(mutex_stub_f *enter, mutex_stub_f *leave)
      : enter(enter), leave(leave) {}

  void lock() { enter(); }
  void unlock() { leave(); }
};

class IReaper {
public:
  virtual ~IReaper() = default;
  virtual EELMutex get_eel_mutex() = 0;
  virtual double ***eel_gmem_attach() = 0;

  virtual std::unique_ptr<IREAPERVideoProcessor> create_video_processor() = 0;

  virtual std::pair<int, int> get_current_project_size(int fallback_width,
                                                       int fallback_height) = 0;

  virtual void print_console(const char *msg) = 0;

  virtual const char *get_ini_file() = 0;

  virtual int plugin_register(const char *name, void *data) = 0;

  static std::unique_ptr<IReaper> get_reaper(const clap::host &host);
};
} // namespace ogler