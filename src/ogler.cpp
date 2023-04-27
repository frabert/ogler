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

#include "ogler.hpp"
#include "compile_shader.hpp"
#include "ogler_editor.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include <nlohmann/json_fwd.hpp>
#include <reaper_plugin_functions.h>

#include <optional>
#include <sstream>
#include <utility>
#include <variant>

namespace ogler {

static constexpr vk::Format RGBAFormat = vk::Format::eB8G8R8A8Unorm;

struct Uniforms {
  float iResolution_w, iResolution_h;
  float iTime;
  float iSampleRate;
  float iChannelResolution_w, iChannelResolution_h;
  float iFrameRate;
  float iWet;
};
static_assert(sizeof(Uniforms) < 128,
              "Keep this under 128 bytes to ensure compatibility!");

union UniformsView {
  Uniforms data;
  std::array<float, sizeof(data) / sizeof(float)> values;
};

struct OglerVst::Compute {
  vk::raii::ShaderModule shader;
  vk::raii::DescriptorSetLayout descriptor_set_layout;
  vk::raii::DescriptorPool descriptor_pool;
  vk::raii::DescriptorSet descriptor_set;

  vk::raii::PipelineCache pipeline_cache;
  vk::raii::PipelineLayout pipeline_layout;
  vk::raii::Pipeline pipeline;

  static vk::raii::DescriptorSetLayout
  create_descriptor_set_layout(VulkanContext &ctx) {
    vk::DescriptorSetLayoutBinding input_texture;
    input_texture.setBinding(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::DescriptorSetLayoutBinding output_texture;
    output_texture.setBinding(1)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eStorageImage)
        .setStageFlags(vk::ShaderStageFlagBits::eCompute);

    std::vector<vk::DescriptorSetLayoutBinding> bindings = {input_texture,
                                                            output_texture};
    vk::DescriptorSetLayoutCreateInfo layout_info;
    layout_info.setBindings(bindings);

    return ctx.device.createDescriptorSetLayout(layout_info);
  }

  static vk::raii::DescriptorPool create_descriptor_pool(VulkanContext &ctx) {
    std::vector<vk::DescriptorPoolSize> pool_sizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 1)};

    vk::DescriptorPoolCreateInfo create_info(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, pool_sizes);
    return ctx.device.createDescriptorPool(create_info);
  }

  static vk::raii::DescriptorSet
  create_descriptor_set(VulkanContext &ctx, vk::raii::DescriptorPool &pool,
                        vk::raii::DescriptorSetLayout &layout) {
    vk::DescriptorSetAllocateInfo alloc_info(*pool, 1, &*layout);
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
                                             pipeline_cache)) {}
};

OglerVst::OglerVst(vst::HostCallback *hostcb)
    : vst::ReaperVstPlugin<OglerVst>(hostcb), sampler(vulkan.create_sampler()),
      command_buffer(vulkan.create_command_buffer()),
      queue(vulkan.get_queue(0)), fence(vulkan.create_fence()),
      output_transfer_buffer(vulkan.create_buffer(
          {}, output_width * output_height * 4,
          vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      output_image(vulkan.create_image(
          output_width, output_height, RGBAFormat, vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eStorage |
              vk::ImageUsageFlagBits::eTransferSrc)),
      output_image_view(vulkan.create_image_view(output_image, RGBAFormat)) {
  if (auto err = recompile_shaders()) {
    DBG << *err << '\n';
  }
}

OglerVst::~OglerVst() = default;

std::string_view OglerVst::get_effect_name() noexcept { return "Ogler"; }
std::string_view OglerVst::get_vendor_name() noexcept {
  return "Francesco Bertolaccini";
}
std::string_view OglerVst::get_product_name() noexcept { return "Ogler"; }
std::int32_t OglerVst::get_vendor_version() noexcept { return 1000; }

void PatchData::deserialize(std::istream &s) {
  nlohmann::json obj;
  s >> obj;

  video_shader = obj["video_shader"];
}

void PatchData::serialize(std::ostream &s) {
  nlohmann::json obj{{"video_shader", video_shader}};
  s << obj;
}

void OglerVst::save_preset_data(std::ostream &s) noexcept { data.serialize(s); }

void OglerVst::load_preset_data(std::istream &s) noexcept {
  data.deserialize(s);
  recompile_shaders();
}

void OglerVst::save_bank_data(std::ostream &s) noexcept { save_preset_data(s); }

void OglerVst::load_bank_data(std::istream &s) noexcept { load_preset_data(s); }

std::optional<std::string> OglerVst::recompile_shaders() {
  auto res = compile_shader({R"(#version 460

layout(local_size_x = 1, local_size_y = 1) in;

layout(push_constant) uniform UniformBlock {
  vec2 iResolution;
  float iTime;
  float iSampleRate;
  vec2 iChannelResolution;
  float iFrameRate;
  float iWet;
};
layout(binding = 0) uniform sampler2D iChannel;
layout(binding = 1, rgba8) uniform writeonly image2D oChannel;)",
                             data.video_shader,
                             R"(void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID));
    imageStore(oChannel, ivec2(gl_GlobalInvocationID), fragColor);
})"});
  if (std::holds_alternative<std::string>(res)) {
    return std::move(std::get<std::string>(res));
  }

  auto code = std::move(std::get<std::vector<unsigned>>(res));

  compute = std::make_unique<Compute>(vulkan, code);

  return {};
}

template <int pixel_size = 4>
static void transfer_image(const char *src, int src_stride, char *dst,
                           int dst_stride, int width, int height) {
  for (int i = 0; i < height; ++i) {
    std::copy(src, src + width * pixel_size, dst);
    src += src_stride;
    dst += dst_stride;
  }
}

struct MemoryMap {
  vk::raii::DeviceMemory &mem;
  std::span<char> ptr;
  MemoryMap(vk::raii::DeviceMemory &mem, int offset, int size) : mem(mem) {
    ptr =
        std::span<char>(static_cast<char *>(mem.mapMemory(offset, size)), size);
  }

  ~MemoryMap() { mem.unmapMemory(); }
};

static void transition_image_layout_upload(vk::raii::CommandBuffer &cmd,
                                           Image &image,
                                           vk::ImageLayout old_layout,
                                           vk::ImageLayout new_layout) {
  vk::ImageMemoryBarrier barrier;
  barrier.setImage(*image.image);
  barrier.setOldLayout(old_layout);
  barrier.setNewLayout(new_layout);
  barrier.setSubresourceRange(
      vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

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

  vk::ImageMemoryBarrier barrier;
  barrier.setImage(*image.image);
  barrier.setOldLayout(old_layout);
  barrier.setNewLayout(new_layout);
  barrier.setSubresourceRange(
      vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  barrier.setSrcAccessMask({});
  barrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);

  sourceStage = vk::PipelineStageFlagBits::eComputeShader;
  destinationStage = vk::PipelineStageFlagBits::eTransfer;

  cmd.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, {barrier});
}

static std::span<char> get_frame_bits(IVideoFrame *frame) {
  return std::span<char>(frame->get_bits(),
                         frame->get_rowspan() * frame->get_h());
}

template <size_t pixel_size = 4>
static void copy_image(std::span<char> src, std::span<char> dst, size_t w,
                       size_t h, size_t src_stride, size_t dst_stride) {
  size_t a = 0;
  size_t b = 0;
  for (size_t i = 0; i < h; ++i) {
    for (size_t j = 0; j < w; ++j) {
      for (size_t k = 0; k < pixel_size; ++k) {
        dst[b + j * pixel_size + k] = src[a + j * pixel_size + k];
      }
    }
    a += src_stride;
    b += dst_stride;
  }
}

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  std::unique_lock<std::mutex> lock(video_mutex, std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return nullptr;
  }

  if (output_frame) {
    output_frame->Release();
  }
  output_frame =
      new_video_frame(output_width, output_height, vst::FrameFormat::RGBA);
  auto output_rowspan = output_frame->get_rowspan();
  auto output_w = output_frame->get_w();
  auto output_h = output_frame->get_h();
  assert(output_w == output_width);
  assert(output_h == output_height);

  UniformsView uniforms{
      .data =
          {
              .iResolution_w = output_width,
              .iResolution_h = output_height,
              .iTime = static_cast<float>(project_time),
              .iSampleRate = 0,
              .iChannelResolution_w = 1,
              .iChannelResolution_h = 1,
              .iFrameRate = static_cast<float>(framerate),
              .iWet = static_cast<float>(parms[0]),
          },
  };

  {
    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    command_buffer.begin(begin_info);
  }

  transition_image_layout_download(command_buffer, output_image);

  auto num_inputs = get_video_num_inputs();
  if (num_inputs > 0) {
    auto input_frame = get_video_input(0, vst::FrameFormat::RGBA);
    auto input_w = input_frame->get_w();
    auto input_h = input_frame->get_h();
    auto input_rowspan = input_frame->get_rowspan();
    auto input_bits = get_frame_bits(input_frame);
    uniforms.data.iChannelResolution_w = input_w;
    uniforms.data.iChannelResolution_h = input_h;
    if (!input_image || input_w != input_image->width ||
        input_h != input_image->height) {
      input_image = vulkan.create_image(
          input_w, input_h, RGBAFormat, vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eSampled |
              vk::ImageUsageFlagBits::eTransferDst);
      input_transfer_buffer = vulkan.create_buffer(
          {}, input_w * input_h * 4, vk::BufferUsageFlagBits::eTransferSrc,
          vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent);
      input_image_view = vulkan.create_image_view(*input_image, RGBAFormat);
    }

    {
      MemoryMap map(input_transfer_buffer->memory, 0,
                    input_transfer_buffer->size);
      copy_image(input_bits, map.ptr, input_w, input_h, input_rowspan,
                 input_w * 4);
    }

    {
      transition_image_layout_upload(command_buffer, *input_image,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal);

      vk::BufferImageCopy region(
          0, 0, 0,
          vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
          vk::Offset3D(0, 0, 0), vk::Extent3D(input_w, input_h, 1));
      command_buffer.copyBufferToImage(
          *input_transfer_buffer->buffer, *input_image->image,
          vk::ImageLayout::eTransferDstOptimal, {region});

      transition_image_layout_upload(command_buffer, *input_image,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal);
    }
  } else if (!input_image || input_image->width * input_image->height != 1) {
    input_image =
        vulkan.create_image(1, 1, RGBAFormat, vk::ImageTiling::eOptimal,
                            vk::ImageUsageFlagBits::eSampled |
                                vk::ImageUsageFlagBits::eTransferDst);
    input_transfer_buffer = vulkan.create_buffer(
        {}, 1 * 1 * 4, vk::BufferUsageFlagBits::eTransferSrc,
        vk::SharingMode::eExclusive,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    input_image_view = vulkan.create_image_view(*input_image, RGBAFormat);
    transition_image_layout_upload(command_buffer, *input_image,
                                   vk::ImageLayout::eUndefined,
                                   vk::ImageLayout::eTransferDstOptimal);
    transition_image_layout_upload(command_buffer, *input_image,
                                   vk::ImageLayout::eTransferDstOptimal,
                                   vk::ImageLayout::eShaderReadOnlyOptimal);
  }

  {
    vk::DescriptorImageInfo input_image_info(
        *sampler, **input_image_view, vk::ImageLayout::eShaderReadOnlyOptimal);
    vk::DescriptorImageInfo output_image_info(*sampler, *output_image_view,
                                              vk::ImageLayout::eGeneral);

    std ::vector<vk::WriteDescriptorSet> write_descriptor_sets = {
        vk::WriteDescriptorSet(*compute->descriptor_set, 0, 0, 1,
                               vk::DescriptorType::eCombinedImageSampler,
                               &input_image_info, nullptr, nullptr),
        vk::WriteDescriptorSet(*compute->descriptor_set, 1, 0, 1,
                               vk::DescriptorType::eStorageImage,
                               &output_image_info, nullptr, nullptr),
    };
    vulkan.device.updateDescriptorSets(write_descriptor_sets, {});
  }

  command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                              *compute->pipeline);
  command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                    *compute->pipeline_layout, 0,
                                    {*compute->descriptor_set}, {});
  command_buffer.pushConstants<float>(*compute->pipeline_layout,
                                      vk::ShaderStageFlagBits::eCompute, 0,
                                      uniforms.values);
  command_buffer.dispatch(output_width, output_height, 1);
  {
    vk::ImageMemoryBarrier img_mem_barrier(
        vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eTransferRead,
        vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, *output_image.image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe,
                                   vk::PipelineStageFlagBits::eTransfer, {}, {},
                                   {}, {img_mem_barrier});
  }
  {
    vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        vk::Offset3D(0, 0, 0),
        vk::Extent3D(output_image.width, output_image.height, 1));
    command_buffer.copyImageToBuffer(*output_image.image,
                                     vk::ImageLayout::eGeneral,
                                     *output_transfer_buffer.buffer, {region});
  }
  {
    vk::BufferMemoryBarrier buf_mem_barrier(
        vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eHostRead,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        *output_transfer_buffer.buffer, 0, VK_WHOLE_SIZE);
    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eHost, {}, {},
                                   {buf_mem_barrier}, {});
  }
  command_buffer.end();

  vk::SubmitInfo SubmitInfo(0,                 // Num Wait Semaphores
                            nullptr,           // Wait Semaphores
                            nullptr,           // Pipeline Stage Flags
                            1,                 // Num Command Buffers
                            &*command_buffer); // List of command buffers
  queue.submit({SubmitInfo}, *fence);
  auto res = vulkan.device.waitForFences({*fence},      // List of fences
                                         true,          // Wait All
                                         uint64_t(-1)); // Timeout
  assert(res == vk::Result::eSuccess);

  {
    MemoryMap map(output_transfer_buffer.memory, 0,
                  output_transfer_buffer.size);
    auto output_bits = get_frame_bits(output_frame);
    copy_image(map.ptr, output_bits, output_w, output_h, output_w * 4,
               output_rowspan);
  }

  vulkan.device.resetFences({*fence});
  command_buffer.reset();

  return output_frame;
}
} // namespace ogler