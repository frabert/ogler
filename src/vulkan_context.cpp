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
  auto ver = VK_MAKE_VERSION(OGLER_VER_MAJOR, OGLER_VER_MINOR, OGLER_VER_REV);
  vk::ApplicationInfo app_info{
      .pApplicationName = "ogler",
      .applicationVersion = ver,
      .pEngineName = "ogler",
      .engineVersion = ver,
      .apiVersion = OGLER_API_VERSION,
  };
  const std::vector<const char *> layers = {
#ifndef NDEBUG
      "VK_LAYER_KHRONOS_validation"
#endif
  };
  vk::InstanceCreateInfo instance_create_info{
      .pApplicationInfo = &app_info,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
  };
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
  vk::DeviceQueueCreateInfo device_queue_create_info{
      .queueFamilyIndex = queue_family_index,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
  };
  vk::DeviceCreateInfo device_create_info{
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &device_queue_create_info,
  };

  return vk::raii::Device(phys_device, device_create_info);
}

static vk::raii::CommandPool create_command_pool(vk::raii::Device &device,
                                                 uint32_t queue_family_index) {
  vk::CommandPoolCreateInfo pool_create_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queue_family_index,
  };
  return vk::raii::CommandPool(device, pool_create_info);
}

VulkanContext::VulkanContext()
    : ctx(), instance(make_instance(ctx)),
      phys_device(std::move(vk::raii::PhysicalDevices(instance).front())),
      queue_family_index(find_queue_family_index(phys_device)),
      device(init_device(phys_device, queue_family_index)),
      command_pool(create_command_pool(device, queue_family_index)) {}

vk::raii::CommandBuffer VulkanContext::create_command_buffer() {
  vk::CommandBufferAllocateInfo command_buffer_alloc_info{
      .commandPool = *command_pool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  return std::move(
      device.allocateCommandBuffers(command_buffer_alloc_info).front());
}

Image VulkanContext::create_image(uint32_t width, uint32_t height,
                                  vk::Format format, vk::ImageTiling tiling,
                                  vk::ImageUsageFlags usage) {
  vk::ImageCreateInfo create_info{
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent =
          {
              .width = width,
              .height = height,
              .depth = 1,
          },
      .mipLevels = 1,
      .arrayLayers = 1,
  };
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
  vk::MemoryAllocateInfo alloc_info{
      .allocationSize = reqs.size,
      .memoryTypeIndex = static_cast<uint32_t>(type_index),
  };

  auto mem = device.allocateMemory(alloc_info);
  image.bindMemory(*mem, 0);
  return Image(std::move(image), std::move(mem), format, width, height);
}

vk::raii::ImageView VulkanContext::create_image_view(Image &img,
                                                     vk::Format format) {
  vk::ImageViewCreateInfo create_info{
      .image = *img.image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange = {
          .aspectMask = vk::ImageAspectFlagBits::eColor,
          .levelCount = 1,
          .layerCount = 1,
      }};
  return device.createImageView(create_info);
}

vk::raii::ShaderModule
VulkanContext::create_shader_module(std::span<const unsigned> code) {
  vk::ShaderModuleCreateInfo create_info{
      .codeSize = code.size_bytes(),
      .pCode = code.data(),
  };
  return device.createShaderModule(create_info);
}

vk::raii::PipelineLayout VulkanContext::create_pipeline_layout(
    vk::raii::DescriptorSetLayout &descriptor_set_layout,
    int push_constants_size) {
  std::vector<vk::PushConstantRange> push_constant_range = {
      vk::PushConstantRange{
          .stageFlags = vk::ShaderStageFlagBits::eCompute,
          .offset = 0,
          .size = static_cast<uint32_t>(push_constants_size),
      },
  };
  std::vector<vk::DescriptorSetLayout> set_layout = {*descriptor_set_layout};
  vk::PipelineLayoutCreateInfo create_info{
      .setLayoutCount = 1,
      .pSetLayouts = set_layout.data(),
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = push_constant_range.data(),
  };
  return device.createPipelineLayout(create_info);
}

vk::raii::PipelineCache VulkanContext::create_pipeline_cache() {
  return device.createPipelineCache(vk::PipelineCacheCreateInfo());
}

vk::raii::Pipeline VulkanContext::create_compute_pipeline(
    vk::raii::ShaderModule &module, const char *entry_point,
    vk::raii::PipelineLayout &pipeline_layout,
    vk::raii::PipelineCache &pipeline_cache,
    vk::SpecializationInfo *spec_info) {
  vk::ComputePipelineCreateInfo create_info{
      .stage =
          {
              .stage = vk::ShaderStageFlagBits::eCompute,
              .module = *module,
              .pName = entry_point,
              .pSpecializationInfo = spec_info,
          },
      .layout = *pipeline_layout,
  };
  return device.createComputePipeline(pipeline_cache, create_info);
}

vk::raii::CommandPool VulkanContext::create_compute_command_pool() {
  vk::CommandPoolCreateInfo create_info{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queue_family_index,
  };
  return device.createCommandPool(create_info);
}

vk::raii::Sampler VulkanContext::create_sampler() {
  vk::SamplerCreateInfo create_info{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
  };
  return device.createSampler(create_info);
}

void VulkanContext::write_descriptor_sets(
    const std::vector<vk::WriteDescriptorSet> &sets) {
  device.updateDescriptorSets(sets, nullptr);
}

vk::raii::Queue VulkanContext::get_queue(uint32_t index) {
  return device.getQueue(queue_family_index, index);
}

vk::raii::Fence VulkanContext::create_fence() { return device.createFence({}); }
} // namespace ogler