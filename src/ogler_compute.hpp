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

#include "ogler.hpp"
#include "ogler_uniforms.hpp"

namespace ogler {
struct SpecializationData {
  unsigned gmem_size;
  int ogler_version_maj;
  int ogler_version_min;
  int ogler_version_rev;
};

struct Ogler::Compute {
  vk::raii::ShaderModule shader;
  vk::raii::DescriptorSetLayout descriptor_set_layout;
  vk::raii::DescriptorPool descriptor_pool;
  vk::raii::DescriptorSet descriptor_set;

  vk::raii::PipelineCache pipeline_cache;
  vk::raii::PipelineLayout pipeline_layout;
  std::array<vk::SpecializationMapEntry, 4> pipeline_spec_entries{
      // ogler_gmem_size
      vk::SpecializationMapEntry{
          .constantID = 0,
          .offset =
              static_cast<uint32_t>(offsetof(SpecializationData, gmem_size)),
          .size = sizeof(SpecializationData::gmem_size),
      },
      // ogler_version_maj
      vk::SpecializationMapEntry{
          .constantID = 1,
          .offset = static_cast<uint32_t>(
              offsetof(SpecializationData, ogler_version_maj)),
          .size = sizeof(SpecializationData::ogler_version_maj),
      },
      // ogler_version_min
      vk::SpecializationMapEntry{
          .constantID = 2,
          .offset = static_cast<uint32_t>(
              offsetof(SpecializationData, ogler_version_min)),
          .size = sizeof(SpecializationData::ogler_version_min),
      },
      // ogler_version_rev
      vk::SpecializationMapEntry{
          .constantID = 3,
          .offset = static_cast<uint32_t>(
              offsetof(SpecializationData, ogler_version_rev)),
          .size = sizeof(SpecializationData::ogler_version_rev),
      },
  };
  SpecializationData pipeline_spec_data{
      .gmem_size = gmem_size,
      .ogler_version_maj = version::major,
      .ogler_version_min = version::minor,
      .ogler_version_rev = version::revision,
  };
  vk::SpecializationInfo pipeline_spec_info{
      .mapEntryCount = static_cast<uint32_t>(pipeline_spec_entries.size()),
      .pMapEntries = pipeline_spec_entries.data(),
      .dataSize = static_cast<uint32_t>(sizeof(pipeline_spec_data)),
      .pData = &pipeline_spec_data,
  };
  vk::raii::Pipeline pipeline;

  static inline vk::raii::DescriptorSetLayout
  create_descriptor_set_layout(VulkanContext &ctx) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        // Input texture
        {
            .binding = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = max_num_inputs,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        // Output texture
        {
            .binding = 2,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        // gmem
        {
            .binding = 3,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        // iChannelResolution[]
        {
            .binding = 4,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        // ogler_previous_frame
        {
            .binding = 5,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
        // Params
        {
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute,
        },
    };
    vk::DescriptorSetLayoutCreateInfo layout_info{
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    return ctx.device.createDescriptorSetLayout(layout_info);
  }

  static inline vk::raii::DescriptorPool
  create_descriptor_pool(VulkanContext &ctx) {
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        // Input texture
        {
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = max_num_inputs,
        },
        // Output texture
        {
            .type = vk::DescriptorType::eStorageImage,
            .descriptorCount = 1,
        },
        // gmem
        {
            .type = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
        },
        // iChannelResolution[]
        {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
        },
        // ogler_previous_frame
        {
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
        },
        // Params
        {
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
        },
    };

    vk::DescriptorPoolCreateInfo create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    return ctx.device.createDescriptorPool(create_info);
  }

  static inline vk::raii::DescriptorSet
  create_descriptor_set(VulkanContext &ctx, vk::raii::DescriptorPool &pool,
                        vk::raii::DescriptorSetLayout &layout) {
    vk::DescriptorSetAllocateInfo alloc_info{
        .descriptorPool = *pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &*layout,
    };
    return std::move(ctx.device.allocateDescriptorSets(alloc_info).front());
  }

  Compute(VulkanContext &ctx, const std::vector<unsigned> &shader_code)
      : shader(ctx.create_shader_module(shader_code)),
        descriptor_set_layout(create_descriptor_set_layout(ctx)),
        descriptor_pool(create_descriptor_pool(ctx)),
        descriptor_set(
            create_descriptor_set(ctx, descriptor_pool, descriptor_set_layout)),
        pipeline_cache(ctx.create_pipeline_cache()),
        pipeline_layout(ctx.create_pipeline_layout(descriptor_set_layout,
                                                   sizeof(Uniforms))),
        pipeline(ctx.create_compute_pipeline(shader, "main", pipeline_layout,
                                             pipeline_cache,
                                             &pipeline_spec_info)) {}
};
} // namespace ogler