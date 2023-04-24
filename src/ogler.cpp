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
#include <vulkan/vulkan_raii.hpp>

namespace ogler {

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

    vk::DescriptorPoolCreateInfo create_info({}, 1, pool_sizes);
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

static constexpr int AlignTo4(int x) { return (x + 3) & 0xFFFFFFFC; }

OglerVst::OglerVst(vst::HostCallback *hostcb)
    : vst::ReaperVstPlugin<OglerVst>(hostcb), sampler(vulkan.create_sampler()),
      command_buffer(vulkan.create_command_buffer()),
      command_pool(vulkan.create_compute_command_pool()),
      output_transfer_buffer(vulkan.create_buffer(
          {}, output_width * output_height * 4,
          vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      output_image(vulkan.create_image(
          output_width, output_height, vk::Format::eB8G8R8A8Unorm,
          vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eStorage |
              vk::ImageUsageFlagBits::eTransferSrc)),
      output_image_view(
          vulkan.create_image_view(output_image, vk::Format::eB8G8R8A8Unorm)) {
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
    mainImage(fragColor, vec2(gl_GlobalInvocationID.xy));
    ivec2 coords = ivec2(gl_GlobalInvocationID.x, gl_NumWorkGroups.y - gl_GlobalInvocationID.y);
    imageStore(oChannel, coords, fragColor);
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
  void *ptr;
  MemoryMap(vk::raii::DeviceMemory &mem, int offset, int size) : mem(mem) {
    ptr = mem.mapMemory(offset, size);
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

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  std::unique_lock<std::mutex> lock(video_mutex, std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return nullptr;
  }

  UniformsView uniforms{
      .data =
          {
              .iResolution_w = output_width,
              .iResolution_h = output_height,
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

  auto num_inputs = get_video_num_inputs();
  if (num_inputs > 0) {
    auto input_frame = get_video_input(0, vst::FrameFormat::RGBA);
    auto input_w = input_frame->get_w();
    auto input_h = input_frame->get_h();
    uniforms.data.iChannelResolution_w = input_w;
    uniforms.data.iChannelResolution_h = input_h;
    if (!input_image || input_w != input_image->width ||
        input_h != input_image->height) {
      input_image =
          vulkan.create_image(input_w, input_h, vk::Format::eB8G8R8A8Unorm,
                              vk::ImageTiling::eOptimal,
                              vk::ImageUsageFlagBits::eSampled |
                                  vk::ImageUsageFlagBits::eTransferDst);
      input_transfer_buffer = vulkan.create_buffer(
          {}, input_w * input_h * 4, vk::BufferUsageFlagBits::eTransferSrc,
          vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent);
      input_image_view =
          vulkan.create_image_view(*input_image, vk::Format::eB8G8R8A8Unorm);

      vk::DescriptorImageInfo input_image_info(
          *sampler, **input_image_view,
          vk::ImageLayout::eShaderReadOnlyOptimal);
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

    {
      MemoryMap map(input_transfer_buffer->memory, 0,
                    input_transfer_buffer->size);
      transfer_image(input_frame->get_bits(), input_frame->get_rowspan(),
                     static_cast<char *>(map.ptr), input_w * 4, input_w,
                     input_h);
    }

    {
      transition_image_layout_upload(command_buffer, *input_image,
                                     vk::ImageLayout::eUndefined,
                                     vk::ImageLayout::eTransferDstOptimal);

      vk::BufferImageCopy region(
          0, 0, 0,
          vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
          vk::Offset3D(0, 0, 0),
          vk::Extent3D(input_image->width, input_image->height, 1));
      command_buffer.copyBufferToImage(
          *input_transfer_buffer->buffer, *input_image->image,
          vk::ImageLayout::eTransferDstOptimal, {region});

      transition_image_layout_upload(command_buffer, *input_image,
                                     vk::ImageLayout::eTransferDstOptimal,
                                     vk::ImageLayout::eShaderReadOnlyOptimal);
    }
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
  command_buffer.end();

  auto queue = vulkan.get_queue(0);
  auto fence = vulkan.create_fence();
  vk::SubmitInfo SubmitInfo(0,                 // Num Wait Semaphores
                            nullptr,           // Wait Semaphores
                            nullptr,           // Pipeline Stage Flags
                            1,                 // Num Command Buffers
                            &*command_buffer); // List of command buffers
  queue.submit({SubmitInfo}, *fence);
  auto res = vulkan.device.waitForFences({*fence},      // List of fences
                                         true,          // Wait All
                                         uint64_t(-1)); // Timeout

  command_buffer.reset();
  return nullptr;
}
} // namespace ogler