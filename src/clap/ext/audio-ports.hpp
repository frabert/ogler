#pragma once

#include <optional>

#include <clap/ext/audio-ports.h>

namespace clap {
template <typename T>
concept AudioPorts = requires(T &plugin, bool is_input, uint32_t index) {
  { plugin.audio_ports_count(is_input) } -> std::same_as<uint32_t>;
  {
    plugin.audio_ports_get(index, is_input)
  } -> std::same_as<std::optional<clap_audio_port_info_t>>;
};

template <AudioPorts Plugin> struct audio_ports {
  static constexpr const char *id = CLAP_EXT_AUDIO_PORTS;

  template <typename Container> static const void *get_ext() {
    static clap_plugin_audio_ports_t audio_ports{
        .count =
            [](const clap_plugin_t *plugin, bool is_input) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              return self->plugin_data.audio_ports_count(is_input);
            },
        .get =
            [](const clap_plugin_t *plugin, uint32_t index, bool is_input,
               clap_audio_port_info_t *info) {
              auto self = static_cast<Container *>(plugin->plugin_data);
              auto res = self->plugin_data.audio_ports_get(index, is_input);
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
};
} // namespace clap