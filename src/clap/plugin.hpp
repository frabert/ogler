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

#include <clap/entry.h>
#include <clap/events.h>
#include <clap/factory/plugin-factory.h>
#include <clap/plugin.h>

#include <concepts>
#include <string_view>

#include "host.hpp"

namespace clap {

template <typename T>
concept Plugin =
    requires(T &plugin, double sample_rate, uint32_t min_frames_count,
             uint32_t max_frames_count, const clap_process_t &process,
             std::string_view ext_id) {
      { T::id } -> std::convertible_to<const char *>;
      { T::name } -> std::convertible_to<const char *>;
      { T::vendor } -> std::convertible_to<const char *>;
      { T::url } -> std::convertible_to<const char *>;
      { T::manual_url } -> std::convertible_to<const char *>;
      { T::support_url } -> std::convertible_to<const char *>;
      { T::version } -> std::convertible_to<const char *>;
      { T::description } -> std::convertible_to<const char *>;
      { T::features } -> std::convertible_to<const char *const *>;
      { plugin.init() } -> std::convertible_to<bool>;
      {
        plugin.activate(sample_rate, min_frames_count, max_frames_count)
      } -> std::convertible_to<bool>;
      { plugin.deactivate() } -> std::same_as<void>;
      { plugin.start_processing() } -> std::convertible_to<bool>;
      { plugin.stop_processing() } -> std::same_as<void>;
      { plugin.reset() } -> std::same_as<void>;
      { plugin.process(process) } -> std::convertible_to<clap_process_status>;
      { plugin.get_extension(ext_id) } -> std::convertible_to<const void *>;
      { plugin.on_main_thread() } -> std::same_as<void>;
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

namespace detail {
template <Plugin T, template <typename> typename... Exts> struct plugin_exts;

template <Plugin T, template <typename> typename Ext,
          template <typename> typename... Exts>
struct plugin_exts<T, Ext, Exts...> {
  template <typename Container>
  static const void *get_ext(std::string_view ext) {
    if (ext == Ext<T>::id) {
      return Ext<T>::get_ext<Container>();
    }
    return plugin_exts<T, Exts...>::get_ext<Container>(ext);
  }
};

template <Plugin T> struct plugin_exts<T> {
  template <typename Container>
  static const void *get_ext(std::string_view ext) {
    return nullptr;
  }
};

template <Plugin T, template <typename> typename... Exts> struct container {
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
                [](const clap_plugin_t *plugin, const clap_process_t *process) {
                  auto self = static_cast<container *>(plugin->plugin_data);
                  return self->plugin_data.process(*process);
                },
            .get_extension = [](const clap_plugin_t *plugin,
                                const char *raw_id) -> const void * {
              auto self = static_cast<container *>(plugin->plugin_data);
              std::string_view id{raw_id};
              auto ext = plugin_exts<T, Exts...>::get_ext<container>(id);
              if (ext == nullptr) {
                return self->plugin_data.get_extension(id);
              } else {
                return ext;
              }
            },
            .on_main_thread =
                [](const clap_plugin_t *plugin) {
                  auto self = static_cast<container *>(plugin->plugin_data);
                  return self->plugin_data.on_main_thread();
                },
        }) {}
};
} // namespace detail

template <typename T, template <typename> typename... Exts>
const clap_plugin_t *create_plugin(const clap_host_t *host) {
  return &(new detail::container<T, Exts...>(
               *static_cast<const ::clap::host *>(host)))
              ->plugin;
};

template <typename T, template <typename> typename... Exts> struct plugin {
  using impl = T;
};

namespace detail {
template <typename... Ts> struct create_plugin;
template <typename T, typename... Ts, template <typename> typename... Exts>
struct create_plugin<plugin<T, Exts...>, Ts...> {
  const clap_plugin_t *operator()(const clap_host_t *host,
                                  std::string_view id) {
    if (id == T::id) {
      return ::clap::create_plugin<T, Exts...>(host);
    } else {
      return ::clap::detail::create_plugin<Ts...>()(host, id);
    }
  }
};
template <typename T, template <typename> typename... Exts>
struct create_plugin<plugin<T, Exts...>> {
  const clap_plugin_t *operator()(const clap_host_t *host,
                                  std::string_view id) {
    if (id == T::id) {
      return ::clap::create_plugin<T, Exts...>(host);
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
            &plugin_descriptor<typename Ts::impl...>::value};
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