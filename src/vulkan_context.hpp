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

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <optional>
#include <span>
#include <utility>

namespace ogler {
struct Image {
  vk::raii::Image image;
  vk::raii::DeviceMemory memory;
  vk::Format format;

  int width;
  int height;

  Image(vk::raii::Image &&img, vk::raii::DeviceMemory &&mem, vk::Format fmt,
        int w, int h)
      : image(std::move(img)), memory(std::move(mem)), format(fmt), width(w),
        height(h) {}
};

template <typename T = char> struct Buffer {
  vk::raii::Buffer buffer;
  vk::raii::DeviceMemory memory;
  std::span<T> map;

  int size;

  Buffer(vk::raii::Buffer &&buf, vk::raii::DeviceMemory &&mem, int sz,
         bool do_map)
      : buffer(std::move(buf)), memory(std::move(mem)), size(sz),
        map(do_map
                ? std::span<T>(
                      static_cast<T *>(memory.mapMemory(0, sz * sizeof(T))), sz)
                : std::span<T>()) {}
};

class VulkanContext {
public:
  vk::raii::Context ctx;
  vk::raii::Instance instance;
  vk::raii::PhysicalDevice phys_device;
  uint32_t queue_family_index;
  vk::raii::Device device;
  vk::raii::CommandPool command_pool;

  std::optional<vk::raii::DebugUtilsMessengerEXT> debug_messenger;

  VulkanContext();

  template <typename T>
  Buffer<T> create_buffer(vk::BufferCreateFlags create_flags,
                          vk::DeviceSize size, vk::BufferUsageFlags usage_flags,
                          vk::SharingMode sharing_mode,
                          vk::MemoryPropertyFlags properties, bool map = true) {
    vk::BufferCreateInfo info{
        .flags = create_flags,
        .size = size * sizeof(T),
        .usage = usage_flags,
        .sharingMode = sharing_mode,
    };
    auto props = phys_device.getMemoryProperties();
    auto type_index = std::distance(
        props.memoryTypes.begin(),
        std::find_if(props.memoryTypes.begin(), props.memoryTypes.end(),
                     [properties](const auto &p) {
                       return (p.propertyFlags & properties) == properties;
                     }));
    auto buf = device.createBuffer(info);
    auto reqs = buf.getMemoryRequirements();
    vk::MemoryAllocateInfo alloc_info{
        .allocationSize = reqs.size,
        .memoryTypeIndex = static_cast<uint32_t>(type_index),
    };

    auto mem = device.allocateMemory(alloc_info);
    buf.bindMemory(*mem, 0);

    return Buffer<T>(std::move(buf), std::move(mem), size, map);
  }

  vk::raii::CommandBuffer create_command_buffer();

  Image create_image(uint32_t width, uint32_t height, vk::Format format,
                     vk::ImageTiling tiling, vk::ImageUsageFlags usage);

  vk::raii::ImageView create_image_view(Image &img, vk::Format format);

  vk::raii::ShaderModule create_shader_module(std::span<const unsigned> code);

  vk::raii::PipelineLayout
  create_pipeline_layout(vk::raii::DescriptorSetLayout &descriptor_set_layout,
                         int push_constants_size);

  vk::raii::PipelineCache create_pipeline_cache();

  vk::raii::Pipeline
  create_compute_pipeline(vk::raii::ShaderModule &module,
                          const char *entry_point,
                          vk::raii::PipelineLayout &pipeline_layout,
                          vk::raii::PipelineCache &pipeline_cache,
                          vk::SpecializationInfo *spec_info = nullptr);

  vk::raii::CommandPool create_compute_command_pool();

  vk::raii::Sampler create_sampler();

  void write_descriptor_sets(const std::vector<vk::WriteDescriptorSet> &sets);

  vk::raii::Queue get_queue(uint32_t index);

  vk::raii::Fence create_fence();
};
} // namespace ogler