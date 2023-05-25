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

#include <clap/entry.h>
#include <clap/events.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/gui.h>
#include <clap/ext/params.h>
#include <clap/ext/state.h>
#include <clap/factory/plugin-factory.h>
#include <clap/plugin.h>

#include <iostream>
#include <optional>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>

#include "host.hpp"

namespace clap {

template <typename Elem = char, typename Traits = std::char_traits<Elem>>
class istreambuf : public std::basic_streambuf<Elem, Traits> {
  const clap_istream_t *stream;

public:
  using char_type = Elem;
  using traits_type = Traits;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

  istreambuf(const clap_istream_t *stream) : stream(stream) {}

protected:
  int_type underflow() final { return uflow(); }

  int_type uflow() final {
    int_type value{};
    auto num_read = stream->read(stream, &value, 1);
    if (num_read < 0) {
      throw std::runtime_error("IO error");
    } else if (num_read == 0) {
      return traits_type::eof();
    } else {
      return value;
    }
  }
};

template <typename Elem = char, typename Traits = std::char_traits<Elem>>
class ostreambuf : public std::basic_streambuf<Elem, Traits> {
  const clap_ostream_t *stream;

public:
  using char_type = Elem;
  using traits_type = Traits;
  using int_type = typename traits_type::int_type;
  using pos_type = typename traits_type::pos_type;
  using off_type = typename traits_type::off_type;

  ostreambuf(const clap_ostream_t *stream) : stream(stream) {}

protected:
  int_type overflow(int_type ch) final {
    if (ch != traits_type::eof()) {
      if (stream->write(stream, &ch, 1) == 1) {
        return ch;
      }
    }
    return traits_type::eof();
  }
};

template <typename T> struct plugin_descriptor {
  static constexpr clap_plugin_descriptor_t value{
      .clap_version = CLAP_VERSION,
      .id = T::id,
      .name = T::name,
      .vendor = T::vendor,
      .url = T::url,
      .manual_url = T::manual_url,
      .support_url = T::support_url,
      .version = T::version,
      .description = T::description,
      .features = T::features,
  };
};

template <typename T>
concept SupportsState =
    requires(T &plugin, std::istream &is, std::ostream &os) {
      { plugin.state_save(os) } -> std::same_as<bool>;
      { plugin.state_load(is) } -> std::same_as<bool>;
    };

template <typename T>
concept SupportsParams =
    requires(T &plugin, uint32_t param_index, clap_id param_id, double value,
             std::span<char> text_out, std::string_view text,
             const clap_input_events_t &in, const clap_output_events_t &out) {
      { plugin.params_count() } -> std::same_as<uint32_t>;
      {
        plugin.params_get_info(param_index)
      } -> std::same_as<std::optional<clap_param_info_t>>;
      {
        plugin.params_get_value(param_id)
      } -> std::same_as<std::optional<double>>;
      {
        plugin.params_value_to_text(param_id, value, text_out)
      } -> std::same_as<bool>;
      {
        plugin.params_text_to_value(param_id, text)
      } -> std::same_as<std::optional<double>>;
      { plugin.params_flush(in, out) } -> std::same_as<void>;
    };

template <typename T>
concept SupportsGui =
    requires(T &plugin, std::string_view api, bool is_floating, double scale,
             uint32_t &r_width, uint32_t &r_height, uint32_t width,
             uint32_t height, const clap_window_t &window) {
      { plugin.gui_is_api_supported(api, is_floating) } -> std::same_as<bool>;
      {
        plugin.gui_get_preferred_api()
      } -> std::same_as<std::optional<std::pair<const char *, bool>>>;
      { plugin.gui_create(api, is_floating) } -> std::same_as<bool>;
      { plugin.gui_destroy() } -> std::same_as<void>;
      { plugin.gui_set_scale(scale) } -> std::same_as<bool>;
      {
        plugin.gui_get_size()
      } -> std::same_as<std::optional<std::pair<uint32_t, uint32_t>>>;
      { plugin.gui_can_resize() } -> std::same_as<bool>;
      {
        plugin.gui_get_resize_hints()
      } -> std::same_as<std::optional<clap_gui_resize_hints_t>>;
      { plugin.gui_adjust_size(r_width, r_height) } -> std::same_as<bool>;
      { plugin.gui_set_size(width, height) } -> std::same_as<bool>;
      { plugin.gui_set_parent(window) } -> std::same_as<bool>;
      { plugin.gui_set_transient(window) } -> std::same_as<bool>;
      { plugin.gui_suggest_title(api) } -> std::same_as<void>;
      { plugin.gui_show() } -> std::same_as<bool>;
      { plugin.gui_hide() } -> std::same_as<bool>;
    };

template <typename T>
concept SupportsAudioPorts =
    requires(T &plugin, bool is_input, uint32_t index) {
      { plugin.audio_ports_count(is_input) } -> std::same_as<uint32_t>;
      {
        plugin.audio_ports_get(index, is_input)
      } -> std::same_as<std::optional<clap_audio_port_info_t>>;
    };

template <typename T>
const clap_plugin_t *create_plugin(const clap_host_t *host) {
  struct container {
    T plugin_data;
    clap_plugin_t plugin;
    container(const ::clap::host &host)
        : plugin_data(host),
          plugin({
              .desc = &plugin_descriptor<T>::value,
              .plugin_data = this,
              .init =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.init();
                  },
              .destroy =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    delete self;
                  },
              .activate =
                  [](const clap_plugin_t *plugin, double sample_rate,
                     uint32_t min_frames_count, uint32_t max_frames_count) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.activate(
                        sample_rate, min_frames_count, max_frames_count);
                  },
              .deactivate =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.deactivate();
                  },
              .start_processing =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.start_processing();
                  },
              .stop_processing =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.stop_processing();
                  },
              .reset =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.reset();
                  },
              .process =
                  [](const clap_plugin_t *plugin,
                     const clap_process_t *process) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.process(*process);
                  },
              .get_extension = [](const clap_plugin_t *plugin,
                                  const char *raw_id) -> const void * {
                auto self = static_cast<container *>(plugin->plugin_data);
                std::string_view id{raw_id};
                if (SupportsState<T> && id == CLAP_EXT_STATE) {
                  static clap_plugin_state_t state{
                      .save =
                          [](const clap_plugin_t *plugin,
                             const clap_ostream_t *stream) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            ostreambuf ostreambuf(stream);
                            std::ostream os(&ostreambuf);
                            return self->plugin_data.state_save(os);
                          },
                      .load =
                          [](const clap_plugin_t *plugin,
                             const clap_istream_t *stream) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            istreambuf istreambuf(stream);
                            std::istream is(&istreambuf);
                            return self->plugin_data.state_load(is);
                          },
                  };
                  return &state;
                } else if (SupportsParams<T> && id == CLAP_EXT_PARAMS) {
                  static clap_plugin_params_t params{
                      .count =
                          [](const clap_plugin_t *plugin) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.params_count();
                          },
                      .get_info =
                          [](const clap_plugin_t *plugin, uint32_t param_index,
                             clap_param_info_t *param_info) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            auto info =
                                self->plugin_data.params_get_info(param_index);
                            if (info) {
                              *param_info = *info;
                              return true;
                            } else {
                              return false;
                            }
                          },
                      .get_value =
                          [](const clap_plugin_t *plugin, clap_id param_id,
                             double *out_value) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            auto value =
                                self->plugin_data.params_get_value(param_id);
                            if (value) {
                              *out_value = *value;
                              return true;
                            } else {
                              return false;
                            }
                          },
                      .value_to_text =
                          [](const clap_plugin_t *plugin, clap_id param_id,
                             double value, char *out_buffer,
                             uint32_t out_buffer_capacity) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.params_value_to_text(
                                param_id, value,
                                std::span<char>(out_buffer,
                                                out_buffer_capacity));
                          },
                      .text_to_value =
                          [](const clap_plugin_t *plugin, clap_id param_id,
                             const char *param_value_text, double *out_value) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            std::string_view text{param_value_text};
                            auto value = self->plugin_data.params_text_to_value(
                                param_id, text);
                            if (value) {
                              *out_value = *value;
                              return true;
                            } else {
                              return false;
                            }
                          },
                      .flush =
                          [](const clap_plugin_t *plugin,
                             const clap_input_events_t *in,
                             const clap_output_events_t *out) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.params_flush(*in, *out);
                          },
                  };
                  return &params;
                } else if (SupportsGui<T> && id == CLAP_EXT_GUI) {
                  static clap_plugin_gui gui{
                      .is_api_supported =
                          [](const clap_plugin_t *plugin, const char *api,
                             bool is_floating) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_is_api_supported(
                                std::string_view{api}, is_floating);
                          },
                      .get_preferred_api =
                          [](const clap_plugin_t *plugin, const char **api,
                             bool *is_floating) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            auto value =
                                self->plugin_data.gui_get_preferred_api();
                            if (value) {
                              *api = std::get<0>(*value);
                              *is_floating = std::get<1>(*value);
                              return true;
                            } else {
                              return false;
                            }
                          },
                      .create =
                          [](const clap_plugin_t *plugin, const char *api,
                             bool is_floating) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_create(
                                std::string_view{api}, is_floating);
                          },
                      .destroy =
                          [](const clap_plugin_t *plugin) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_destroy();
                          },
                      .set_scale =
                          [](const clap_plugin_t *plugin, double scale) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_set_scale(scale);
                          },
                      .get_size =
                          [](const clap_plugin_t *plugin, uint32_t *width,
                             uint32_t *height) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            auto value = self->plugin_data.gui_get_size();
                            if (value) {
                              *width = std::get<0>(*value);
                              *height = std::get<1>(*value);
                              return true;
                            } else {
                              return false;
                            }
                          },
                      .can_resize =
                          [](const clap_plugin_t *plugin) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_can_resize();
                          },
                      .get_resize_hints =
                          [](const clap_plugin_t *plugin,
                             clap_gui_resize_hints_t *hints) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            auto value =
                                self->plugin_data.gui_get_resize_hints();
                            if (value) {
                              *hints = *value;
                              return true;
                            } else {
                              return false;
                            }
                          },
                      .adjust_size =
                          [](const clap_plugin_t *plugin, uint32_t *width,
                             uint32_t *height) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_adjust_size(*width,
                                                                     *height);
                          },
                      .set_size =
                          [](const clap_plugin_t *plugin, uint32_t width,
                             uint32_t height) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_set_size(width,
                                                                  height);
                          },
                      .set_parent =
                          [](const clap_plugin_t *plugin,
                             const clap_window_t *window) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_set_parent(*window);
                          },
                      .set_transient =
                          [](const clap_plugin_t *plugin,
                             const clap_window_t *window) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_set_transient(*window);
                          },
                      .suggest_title =
                          [](const clap_plugin_t *plugin, const char *title) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_suggest_title(
                                std::string_view{title});
                          },
                      .show =
                          [](const clap_plugin_t *plugin) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_show();
                          },
                      .hide =
                          [](const clap_plugin_t *plugin) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.gui_hide();
                          },
                  };
                  return &gui;
                } else if (SupportsAudioPorts<T> &&
                           id == CLAP_EXT_AUDIO_PORTS) {
                  static clap_plugin_audio_ports_t audio_ports{
                      .count =
                          [](const clap_plugin_t *plugin, bool is_input) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            return self->plugin_data.audio_ports_count(
                                is_input);
                          },
                      .get =
                          [](const clap_plugin_t *plugin, uint32_t index,
                             bool is_input, clap_audio_port_info_t *info) {
                            auto self =
                                static_cast<container *>(plugin->plugin_data);
                            auto res = self->plugin_data.audio_ports_get(
                                index, is_input);
                            if (res) {
                              *info = *res;
                              return true;
                            } else {
                              return false;
                            }
                          },
                  };
                  return &audio_ports;
                }
                return self->plugin_data.get_extension(id);
              },
              .on_main_thread =
                  [](const clap_plugin_t *plugin) {
                    auto self = static_cast<container *>(plugin->plugin_data);
                    return self->plugin_data.on_main_thread();
                  },
          }) {}
  };
  return &(new container(*static_cast<const ::clap::host *>(host)))->plugin;
};

namespace detail {
template <typename... Ts> struct create_plugin;
template <typename T, typename... Ts> struct create_plugin<T, Ts...> {
  const clap_plugin_t *operator()(const clap_host_t *host,
                                  std::string_view id) {
    if (id == T::id) {
      return ::clap::create_plugin<T>(host);
    } else {
      return ::clap::detail::create_plugin<Ts...>()(host, id);
    }
  }
};
template <typename T> struct create_plugin<T> {
  const clap_plugin_t *operator()(const clap_host_t *host,
                                  std::string_view id) {
    if (id == T::id) {
      return ::clap::create_plugin<T>(host);
    } else {
      return nullptr;
    }
  }
};
}; // namespace detail

template <typename... Ts> struct plugin_factory {
  static constexpr clap_plugin_factory_t value{
      .get_plugin_count = [](const clap_plugin_factory_t *) -> uint32_t {
        return sizeof...(Ts);
      },
      .get_plugin_descriptor = [](const clap_plugin_factory_t *, uint32_t index)
          -> const clap_plugin_descriptor_t * {
        if (index >= sizeof...(Ts)) {
          return nullptr;
        }
        const clap_plugin_descriptor_t *descs[] = {
            &plugin_descriptor<Ts...>::value};
        return descs[index];
      },
      .create_plugin = [](const clap_plugin_factory *, const clap_host_t *host,
                          const char *plugin_id) -> const clap_plugin_t * {
        std::string_view id(plugin_id);
        return clap::detail::create_plugin<Ts...>()(host, id);
      },
  };

  static const void *getter(const char *factory_id) {
    if (std::string_view{factory_id} != CLAP_PLUGIN_FACTORY_ID) {
      return nullptr;
    }

    return &value;
  }
};
} // namespace clap