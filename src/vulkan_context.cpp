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

#include "vulkan_context.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define OGLER_CONCAT_(x, y) x##y
#define OGLER_CONCAT(x, y) OGLER_CONCAT_(x, y)
#define OGLER_API_VERSION OGLER_CONCAT(VK_API_VERSION_, OGLER_VULKAN_VER)

namespace ogler {

static vk::raii::Instance make_instance(vk::raii::Context &ctx) {
  vk::ApplicationInfo app_info("ogler", VK_MAKE_VERSION(1, 0, 0), "ogler",
                               VK_MAKE_VERSION(1, 0, 0), OGLER_API_VERSION);
  vk::InstanceCreateInfo instance_create_info({}, &app_info, 0, nullptr, 0,
                                              nullptr);
  return vk::raii::Instance(ctx, instance_create_info);
}

static uint32_t find_queue_family_index(vk::raii::PhysicalDevice &phys_device) {
  auto queue_props = phys_device.getQueueFamilyProperties();
  return std::distance(queue_props.begin(),
                       std::find_if(queue_props.begin(), queue_props.end(),
                                    [](const vk::QueueFamilyProperties &q) {
                                      return q.queueFlags &
                                             vk::QueueFlagBits::eCompute;
                                    }));
}

static vk::raii::Device init_device(vk::raii::PhysicalDevice &phys_device,
                                    uint32_t queue_family_index) {
  float queue_priority = 0.0f;
  vk::DeviceQueueCreateInfo device_queue_create_info({}, queue_family_index, 1,
                                                     &queue_priority);
  vk::DeviceCreateInfo device_create_info({}, device_queue_create_info);

  return vk::raii::Device(phys_device, device_create_info);
}

static vk::raii::CommandPool create_command_pool(vk::raii::Device &device,
                                                 uint32_t queue_family_index) {
  vk::CommandPoolCreateInfo pool_create_info({}, queue_family_index);
  return vk::raii::CommandPool(device, pool_create_info);
}

VulkanContext::VulkanContext()
    : ctx(), instance(make_instance(ctx)),
      phys_device(std::move(vk::raii::PhysicalDevices(instance).front())),
      queue_family_index(find_queue_family_index(phys_device)),
      device(init_device(phys_device, queue_family_index)),
      command_pool(create_command_pool(device, queue_family_index)) {}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
VulkanContext::create_buffer(vk::BufferCreateFlags create_flags,
                             vk::DeviceSize size,
                             vk::BufferUsageFlags usage_flags,
                             vk::SharingMode sharing_mode,
                             vk::MemoryPropertyFlags properties) {
  vk::BufferCreateInfo info(create_flags, size, usage_flags, sharing_mode);
  auto props = phys_device.getMemoryProperties();
  auto type_index = std::distance(
      props.memoryTypes.begin(),
      std::find_if(props.memoryTypes.begin(), props.memoryTypes.end(),
                   [properties](const auto &p) {
                     return (p.propertyFlags & properties) == properties;
                   }));
  auto buf = device.createBuffer(info);
  auto reqs = buf.getMemoryRequirements();
  vk::MemoryAllocateInfo alloc_info(reqs.size, type_index);

  auto mem = device.allocateMemory(alloc_info);
  buf.bindMemory(*mem, 0);

  return {std::move(buf), std::move(mem)};
}

vk::raii::CommandBuffer VulkanContext::create_command_buffer() {
  vk::CommandBufferAllocateInfo command_buffer_alloc_info(
      *command_pool, vk::CommandBufferLevel::ePrimary, 1);
  return std::move(
      device.allocateCommandBuffers(command_buffer_alloc_info).front());
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory>
VulkanContext::create_image(uint32_t width, uint32_t height, vk::Format format,
                            vk::ImageTiling tiling, vk::ImageUsageFlags usage) {
  vk::ImageCreateInfo create_info({}, vk::ImageType::e2D, format,
                                  vk::Extent3D(width, height, 1), 1, 1);
  create_info.tiling = tiling;
  create_info.usage = usage;

  auto image = device.createImage(create_info);

  auto props = phys_device.getMemoryProperties();
  auto type_index = std::distance(
      props.memoryTypes.begin(),
      std::find_if(props.memoryTypes.begin(), props.memoryTypes.end(),
                   [](const vk::MemoryType &p) {
                     return p.propertyFlags &
                            vk::MemoryPropertyFlagBits::eDeviceLocal;
                   }));
  auto reqs = image.getMemoryRequirements();
  vk::MemoryAllocateInfo alloc_info(reqs.size, type_index);

  auto mem = device.allocateMemory(alloc_info);
  image.bindMemory(*mem, 0);

  return {std::move(image), std::move(mem)};
}

vk::raii::ShaderModule
VulkanContext::create_shader_module(std::span<unsigned int> code) {
  vk::ShaderModuleCreateInfo create_info({}, code.size_bytes(), code.data());
  return device.createShaderModule(create_info);
}
} // namespace ogler