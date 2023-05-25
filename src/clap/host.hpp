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

#pragma once

#include <clap/ext/audio-ports.h>
#include <clap/ext/gui.h>
#include <clap/ext/log.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/host.h>

namespace clap {
struct host final : clap_host_t {
  inline void request_restart() const {
    static_cast<const clap_host_t *>(this)->request_restart(this);
  }

  inline void request_process() const {
    static_cast<const clap_host_t *>(this)->request_process(this);
  }

  inline void request_callback() const {
    static_cast<const clap_host_t *>(this)->request_callback(this);
  }

  template <typename T> inline const T *get_extension(const char *id) const {
    return static_cast<const T *>(
        static_cast<const clap_host_t *>(this)->get_extension(this, id));
  }

  inline void state_mark_dirty() const {
    auto state = get_extension<clap_host_state_t>(CLAP_EXT_STATE);
    state->mark_dirty(this);
  }

  inline void params_rescan(clap_param_rescan_flags flags) const {
    auto params = get_extension<clap_host_params_t>(CLAP_EXT_STATE);
    params->rescan(this, flags);
  }

  inline void params_clear(clap_id param_id,
                           clap_param_clear_flags flags) const {
    auto params = get_extension<clap_host_params_t>(CLAP_EXT_STATE);
    params->clear(this, param_id, flags);
  }

  inline void params_request_flush() const {
    auto params = get_extension<clap_host_params_t>(CLAP_EXT_STATE);
    params->request_flush(this);
  }

  inline void gui_resize_hints_changed() const {
    auto gui = get_extension<clap_host_gui_t>(CLAP_EXT_STATE);
    gui->resize_hints_changed(this);
  }

  inline void gui_request_resize(uint32_t width, uint32_t height) const {
    auto gui = get_extension<clap_host_gui_t>(CLAP_EXT_STATE);
    gui->request_resize(this, width, height);
  }

  inline void gui_request_show() const {
    auto gui = get_extension<clap_host_gui_t>(CLAP_EXT_STATE);
    gui->request_show(this);
  }

  inline void gui_request_hide() const {
    auto gui = get_extension<clap_host_gui_t>(CLAP_EXT_STATE);
    gui->request_hide(this);
  }

  inline void gui_closed(bool was_destroyed) const {
    auto gui = get_extension<clap_host_gui_t>(CLAP_EXT_STATE);
    gui->closed(this, was_destroyed);
  }

  inline bool audio_ports_is_rescan_flag_supported(uint32_t flag) const {
    auto audio_ports =
        get_extension<clap_host_audio_ports_t>(CLAP_EXT_AUDIO_PORTS);
    return audio_ports->is_rescan_flag_supported(this, flag);
  }

  inline void audio_ports_rescan(uint32_t flags) const {
    auto audio_ports =
        get_extension<clap_host_audio_ports_t>(CLAP_EXT_AUDIO_PORTS);
    audio_ports->rescan(this, flags);
  }

  inline void log(clap_log_severity severity, const char *msg) const {
    auto _log = get_extension<clap_host_log_t>(CLAP_EXT_LOG);
    _log->log(this, severity, msg);
  }
};
static_assert(sizeof(host) == sizeof(clap_host_t));
} // namespace clap