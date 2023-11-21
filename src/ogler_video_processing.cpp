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

#include "ogler.hpp"
#include "ogler_compute.hpp"
#include "ogler_uniforms.hpp"
#include "video_frame.h"

#include <algorithm>
#include <atomic>
#include <vulkan/vulkan_raii.hpp>

namespace ogler {

static void transition_image_layout_upload(vk::raii::CommandBuffer &cmd,
                                           Image &image,
                                           vk::ImageLayout old_layout,
                                           vk::ImageLayout new_layout) {
  vk::ImageMemoryBarrier barrier{
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .image = *image.image,
      .subresourceRange =
          {
              .aspectMask = vk::ImageAspectFlagBits::eColor,
              .levelCount = 1,
              .layerCount = 1,
          },
  };

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (old_layout == vk::ImageLayout::eUndefined &&
      new_layout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.setSrcAccessMask({});
    barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
             new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eComputeShader;
  }

  cmd.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, {barrier});
}

static void transition_image_layout_download(vk::raii::CommandBuffer &cmd,
                                             Image &image) {
  auto old_layout = vk::ImageLayout::eUndefined;
  auto new_layout = vk::ImageLayout::eGeneral;

  vk::ImageMemoryBarrier barrier{
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .image = *image.image,
      .subresourceRange =
          {
              .aspectMask = vk::ImageAspectFlagBits::eColor,
              .levelCount = 1,
              .layerCount = 1,
          },
  };

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  barrier.setSrcAccessMask({});
  barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);

  sourceStage = vk::PipelineStageFlagBits::eComputeShader;
  destinationStage = vk::PipelineStageFlagBits::eTransfer;

  cmd.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, {barrier});
}

InputImage Ogler::create_input_image(int w, int h) {
  auto img = shared.vulkan.create_image(
      w, h, RGBAFormat, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
  auto buf = shared.vulkan.create_buffer<char>(
      {}, w * h * 4, vk::BufferUsageFlagBits::eTransferSrc,
      vk::SharingMode::eExclusive,
      vk::MemoryPropertyFlagBits::eHostVisible |
          vk::MemoryPropertyFlagBits::eHostCoherent);
  auto view = shared.vulkan.create_image_view(img, RGBAFormat);

  return {
      .image = std::move(img),
      .transfer_buffer = std::move(buf),
      .view = std::move(view),
  };
}

bool Ogler::init() {
  eel_mutex = reaper->get_eel_mutex();

  one_shot_execute([&]() {
    transition_image_layout_download(command_buffer, output_image);
    transition_image_layout_download(command_buffer, previous_image);
  });

  gmem = reaper->eel_gmem_attach();

  return true;
}

void Ogler::update_frame_buffers() noexcept {
  auto old_width = output_image.width;
  auto old_height = output_image.height;

  auto new_width = get_output_width();
  auto new_height = get_output_height();

  if (new_width != old_width || new_height != old_height) {
    output_transfer_buffer = shared.vulkan.create_buffer<char>(
        {}, new_width * new_height * 4, vk::BufferUsageFlagBits::eTransferDst,
        vk::SharingMode::eExclusive,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    output_image = shared.vulkan.create_image(
        new_width, new_height, RGBAFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eSampled);
    output_image_view =
        shared.vulkan.create_image_view(output_image, RGBAFormat);

    previous_image = shared.vulkan.create_image(
        new_width, new_height, RGBAFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eSampled);
    previous_image_view =
        shared.vulkan.create_image_view(previous_image, RGBAFormat);

    one_shot_execute([&]() {
      transition_image_layout_download(command_buffer, output_image);
      transition_image_layout_download(command_buffer, previous_image);
    });
  }
}

static std::span<char> get_frame_bits(IVideoFrame *frame) {
  return std::span<char>(frame->get_bits(),
                         frame->get_rowspan() * frame->get_h());
}

template <size_t pixel_size = 4>
static void copy_image(std::span<char> src_span, std::span<char> dst_span,
                       size_t w, size_t h, size_t src_stride,
                       size_t dst_stride) {
  char *src = src_span.data();
  char *dst = dst_span.data();
  for (size_t i = 0; i < h; ++i) {
    std::memcpy(dst, src, w * pixel_size);
    src += src_stride;
    dst += dst_stride;
  }
}

IVideoFrame *Ogler::video_process_frame(std::span<const double> parms,
                                        double project_time, double framerate,
                                        FrameFormat force_format) noexcept {
  std::unique_lock<std::mutex> lock(video_mutex, std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return nullptr;
  }

  if (!compute) {
    return nullptr;
  }

  update_frame_buffers();

  output_frame = vproc->newVideoFrame(output_image.width, output_image.height,
                                      (int)FrameFormat::RGBA);
  auto output_rowspan = output_frame->get_rowspan();
  auto output_w = output_frame->get_w();
  auto output_h = output_frame->get_h();
  auto num_inputs = vproc->getNumInputs();

  UniformsView uniforms{
      .data =
          {
              .iResolution_w = static_cast<float>(output_image.width),
              .iResolution_h = static_cast<float>(output_image.height),
              .iTime = static_cast<float>(project_time),
              .iSampleRate = 0,
              .iFrameRate = static_cast<float>(framerate),
              .iWet = static_cast<float>(parms[0]),
              .num_inputs =
                  std::min({static_cast<int>(max_num_inputs), num_inputs}),
          },
  };

  {
    vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    command_buffer.begin(begin_info);
  }

  {
    std::unique_lock<EELMutex> eel_lock(*eel_mutex);
    auto dst = shared.gmem_transfer_buffer.map.data();
    double **pblocks = *gmem;
    if (pblocks) {
      for (size_t i = 0; i < NSEEL_RAM_BLOCKS; ++i) {
        auto buf = pblocks[i];
        if (buf) {
          for (size_t j = 0; j < NSEEL_RAM_ITEMSPERBLOCK; ++j) {
            dst[i * NSEEL_RAM_ITEMSPERBLOCK + j] = buf[j];
          }

          command_buffer.copyBuffer(
              *shared.gmem_transfer_buffer.buffer, *shared.gmem_buffer.buffer,
              {
                  {
                      .srcOffset = i * sizeof(float) * NSEEL_RAM_ITEMSPERBLOCK,
                      .dstOffset = i * sizeof(float) * NSEEL_RAM_ITEMSPERBLOCK,
                      .size = sizeof(float) * NSEEL_RAM_ITEMSPERBLOCK,
                  },
              });
        }
      }

      command_buffer.pipelineBarrier(
          vk::PipelineStageFlagBits::eHost,
          vk::PipelineStageFlagBits::eComputeShader, {}, {},
          {
              vk::BufferMemoryBarrier{
                  .srcAccessMask = vk::AccessFlagBits::eHostWrite,
                  .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                  .buffer = *shared.gmem_buffer.buffer,
                  .size = VK_WHOLE_SIZE,
              },
          },
          {});
    }
  }

  transition_image_layout_upload(command_buffer, empty_input.image,
                                 vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eTransferDstOptimal);
  transition_image_layout_upload(command_buffer, empty_input.image,
                                 vk::ImageLayout::eTransferDstOptimal,
                                 vk::ImageLayout::eShaderReadOnlyOptimal);

  std::array<std::pair<float, float>, max_num_inputs> input_resolution;
  std::array<vk::DescriptorImageInfo, max_num_inputs> input_image_info;
  size_t n_inputs = 0;
  for (size_t i = 0; i < max_num_inputs; ++i) {
    auto input_frame = vproc->renderInputVideoFrame(i, (int)FrameFormat::RGBA);
    if (!input_frame) {
      input_resolution[i] = {1.f, 1.f};
      input_image_info[i] = {
          .sampler = *sampler,
          .imageView = *empty_input.view,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
      };
    } else {
      auto input_w = input_frame->get_w();
      auto input_h = input_frame->get_h();
      auto input_rowspan = input_frame->get_rowspan();
      auto input_bits = get_frame_bits(input_frame);

      if (n_inputs >= input_images.size()) {
        input_images.push_back(create_input_image(input_w, input_h));
      }

      auto &input_image = input_images[n_inputs];
      ++n_inputs;

      if (input_image.image.width != input_w ||
          input_image.image.height != input_h) {
        input_image = create_input_image(input_w, input_h);
      }

      input_resolution[i] = {static_cast<float>(input_w),
                             static_cast<float>(input_h)};
      input_image_info[i] = {
          .sampler = *sampler,
          .imageView = *input_image.view,
          .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
      };

      copy_image(input_bits, input_image.transfer_buffer.map, input_w, input_h,
                 input_rowspan, input_w * 4);

      {
        transition_image_layout_upload(command_buffer, input_image.image,
                                       vk::ImageLayout::eUndefined,
                                       vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .layerCount = 1,
                },
            .imageExtent =
                {
                    .width = static_cast<uint32_t>(input_w),
                    .height = static_cast<uint32_t>(input_h),
                    .depth = 1,
                },
        };
        command_buffer.copyBufferToImage(
            *input_image.transfer_buffer.buffer, *input_image.image.image,
            vk::ImageLayout::eTransferDstOptimal, {region});

        transition_image_layout_upload(command_buffer, input_image.image,
                                       vk::ImageLayout::eTransferDstOptimal,
                                       vk::ImageLayout::eShaderReadOnlyOptimal);
      }
    }
  }

  {
    vk::DescriptorImageInfo output_image_info{
        .sampler = *sampler,
        .imageView = *output_image_view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };
    vk::DescriptorBufferInfo gmem_buffer_info{
        .buffer = *shared.gmem_buffer.buffer,
        .offset = 0,
        .range = gmem_size * sizeof(float),
    };
    vk::DescriptorBufferInfo input_resolution_info{
        .buffer = *input_resolution_buffer.buffer,
        .offset = 0,
        .range = sizeof(input_resolution),
    };
    vk::DescriptorImageInfo previous_frame_info{
        .sampler = *sampler,
        .imageView = *previous_image_view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    std ::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
        // Input texture
        {
            .dstSet = *compute->descriptor_set,
            .dstBinding = 1,
            .descriptorCount = max_num_inputs,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = input_image_info.data(),
        },
        // Output texture
        {
            .dstSet = *compute->descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .pImageInfo = &output_image_info,
        },
        // gmem
        {
            .dstSet = *compute->descriptor_set,
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &gmem_buffer_info,
        },
        // iChannelResolution[]
        {
            .dstSet = *compute->descriptor_set,
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &input_resolution_info,
        },
        // ogler_previous_frame
        {
            .dstSet = *compute->descriptor_set,
            .dstBinding = 5,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &previous_frame_info,
        },
    };

    std::copy(input_resolution.begin(), input_resolution.end(),
              input_resolution_buffer.map.begin());

    vk::DescriptorBufferInfo uniforms_info{
        .range = sizeof(float) * data.parameters.size(),
    };
    if (params_buffer && !data.parameters.empty()) {
      uniforms_info.buffer = *params_buffer->buffer;
      // keeping in mind parms[0] is iWet
      for (size_t i = 0; i < parms.size() - 1; ++i) {
        params_buffer->map[i] = parms[i + 1];
      }
      write_descriptor_sets.push_back({
          .dstSet = *compute->descriptor_set,
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pBufferInfo = &uniforms_info,
      });
    }

    shared.vulkan.device.updateDescriptorSets(write_descriptor_sets, {});
  }

  command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                              *compute->pipeline);
  command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                    *compute->pipeline_layout, 0,
                                    {*compute->descriptor_set}, {});
  command_buffer.pushConstants<float>(*compute->pipeline_layout,
                                      vk::ShaderStageFlagBits::eCompute, 0,
                                      uniforms.values);
  command_buffer.dispatch(output_image.width, output_image.height, 1);
  {
    vk::ImageMemoryBarrier img_mem_barrier{
        .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = vk::ImageLayout::eGeneral,
        .newLayout = vk::ImageLayout::eGeneral,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = *output_image.image,
        .subresourceRange =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .levelCount = 1,
                .layerCount = 1,
            },
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {},
                                   {}, {img_mem_barrier});
  }
  {
    vk::BufferImageCopy region{
        .imageSubresource =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .layerCount = 1,
            },
        .imageExtent =
            {
                .width = static_cast<uint32_t>(output_image.width),
                .height = static_cast<uint32_t>(output_image.height),
                .depth = 1,
            },
    };
    command_buffer.copyImageToBuffer(*output_image.image,
                                     vk::ImageLayout::eGeneral,
                                     *output_transfer_buffer.buffer, {region});
  }
  {
    vk::BufferMemoryBarrier buf_mem_barrier{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eHostRead,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = *output_transfer_buffer.buffer,
        .size = VK_WHOLE_SIZE,
    };
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eHost, {}, {},
                                   {buf_mem_barrier}, {});
  }
  command_buffer.end();

  vk::SubmitInfo SubmitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &*command_buffer,
  };
  queue.submit({SubmitInfo}, *fence);
  auto res = shared.vulkan.device.waitForFences({*fence},      // List of fences
                                                true,          // Wait All
                                                uint64_t(-1)); // Timeout
  assert(res == vk::Result::eSuccess);

  {
    auto output_bits = get_frame_bits(output_frame);
    copy_image(output_transfer_buffer.map, output_bits, output_w, output_h,
               output_w * 4, output_rowspan);
  }

  shared.vulkan.device.resetFences({*fence});
  command_buffer.reset();

  std::swap(output_image, previous_image);
  std::swap(output_image_view, previous_image_view);

  return output_frame;
}
} // namespace ogler