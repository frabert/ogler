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

#include <vulkan/vulkan_raii.hpp>

#include <utility>

namespace ogler {
class VulkanContext {
public:
  vk::raii::Context ctx;
  vk::raii::Instance instance;
  vk::raii::PhysicalDevice phys_device;
  uint32_t queue_family_index;
  vk::raii::Device device;
  vk::raii::CommandPool command_pool;

  VulkanContext();

  std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
  create_buffer(vk::BufferCreateFlags create_flags, vk::DeviceSize size,
                vk::BufferUsageFlags usage_flags, vk::SharingMode sharing_mode,
                vk::MemoryPropertyFlags properties);

  vk::raii::CommandBuffer create_command_buffer();

  std::pair<vk::raii::Image, vk::raii::DeviceMemory>
  create_image(uint32_t width, uint32_t height, vk::Format format,
               vk::ImageTiling tiling, vk::ImageUsageFlags usage);
};
} // namespace ogler