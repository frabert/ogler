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

#pragma once

#include "VstPlugin.hpp"

#include <WDL/wdltypes.h>

#include <reaper_plugin.h>
#include <reaper_plugin_functions.h>
#include <video_processor.h>

#include <array>
#include <fstream>
#include <memory>
#include <optional>
#include <span>

namespace std {
template <> struct default_delete<IVideoFrame> {
  void operator()(IVideoFrame *frame) const;
};
} // namespace std

namespace vst {

enum class FrameFormat : int {
  Native = 0,
  YV12 = 'YV12',
  RGBA = 'RGBA',
  YUV2 = 'YUY2'
};

extern IREAPERVideoProcessor *(*video_CreateVideoProcessor)(void *fxctx,
                                                            int version);

template <typename T> class ReaperVstPlugin : public VstPlugin<T> {
  std::unique_ptr<IREAPERVideoProcessor> vproc{};

public:
  ReaperVstPlugin(vst::HostCallback *hostcb) : VstPlugin<T>(hostcb) {}

  virtual ~ReaperVstPlugin<T>() = default;

protected:
  void *host_get_reaper_ctx() noexcept {
    return reinterpret_cast<void *>(
        this->hostcb(this->get_effect(), static_cast<HostOpcodes>(0xdeadbeef),
                     0xdeadf00e, 4, NULL, 0.0));
  }

  virtual IVideoFrame *video_process_frame(std::span<const double> parms,
                                           double project_time,
                                           double framerate,
                                           FrameFormat force_format) noexcept {
    return nullptr;
  }

  virtual std::optional<double> video_get_parameter(int index) noexcept {
    if (index < get_num_parameters()) {
      return this->get_parameter(index);
    } else {
      return std::nullopt;
    }
  }

  virtual int get_num_parameters() noexcept { return T::num_params; }

  IVideoFrame *new_video_frame(int width, int height,
                               FrameFormat format) noexcept {
    return vproc->newVideoFrame(width, height, static_cast<int>(format));
  }

  int get_video_num_inputs() noexcept { return vproc->getNumInputs(); }

  IVideoFrame *get_video_input(int index, FrameFormat fmt) noexcept {
    return vproc->renderInputVideoFrame(index, static_cast<int>(fmt));
  }

  virtual Supported can_do(std::string_view s) noexcept override {
    if (s == "hasCockosExtensions") {
      return static_cast<Supported>(0xbeef0000);
    }
    return Supported::No;
  }

  virtual void get_param_range(double &min, double &max) {}

  std::intptr_t vendor_specific(std::int32_t index, std::intptr_t value,
                                void *ptr, float opt) noexcept final {
    if (index == 0xdeadbef0 && ptr && value >= 0 &&
        value < get_num_parameters()) {
      auto range = static_cast<double *>(ptr);
      get_param_range(range[0], range[1]);
      return 0xbeef;
    }

    return 0;
  }

  void adjust_params_num(int start_idx, int num) noexcept {
    int list[] = {start_idx, num};
    this->hostcb(get_effect(), HostOpcodes::VendorSpecific, 0xdeadbeef,
                 HostOpcodes::Automate, list, 0);
  }

  virtual void init() noexcept override {
    void *ctx = host_get_reaper_ctx();
    if (!ctx) {
      return;
    }
    if (!video_CreateVideoProcessor) {
#define OGLER_LOAD(funcname)                                                   \
  funcname = reinterpret_cast<decltype(funcname)>(                             \
      this->hostcb(this->get_effect(), static_cast<HostOpcodes>(0xdeadbeef),   \
                   0xdeadf00d, 0, const_cast<char *>(#funcname), 0.0))
      OGLER_LOAD(video_CreateVideoProcessor);
      OGLER_LOAD(ShowConsoleMsg);
#undef OGLER_LOAD
    }
    vproc = std::unique_ptr<IREAPERVideoProcessor>{video_CreateVideoProcessor(
        ctx, IREAPERVideoProcessor::REAPER_VIDEO_PROCESSOR_VERSION)};
    assert(vproc && "no video processor available");
    vproc->userdata = this;
    vproc->process_frame =
        [](IREAPERVideoProcessor *vproc, const double *parmlist, int nparms,
           double project_time, double frate, int force_format) {
          auto plugin = static_cast<ReaperVstPlugin<T> *>(vproc->userdata);
          return plugin->video_process_frame(
              std::span{parmlist, static_cast<size_t>(nparms)}, project_time,
              frate, static_cast<FrameFormat>(force_format));
        };
    vproc->get_parameter_value = [](IREAPERVideoProcessor *vproc, int idx,
                                    double *valueOut) -> bool {
      auto plugin = static_cast<ReaperVstPlugin<T> *>(vproc->userdata);
      auto val = plugin->video_get_parameter(idx);
      if (val.has_value()) {
        *valueOut = *val;
        return true;
      } else {
        return false;
      }
    };
  }
};

} // namespace vst