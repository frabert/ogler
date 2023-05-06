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
#include "ogler_debug.hpp"
#include "ogler_editor.hpp"
#include "ogler_params.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

#include <nlohmann/json_fwd.hpp>
#include <reaper_plugin_functions.h>

#include <optional>
#include <sstream>
#include <utility>
#include <variant>

#include <WDL/eel2/ns-eel.h>

#undef min

namespace ogler {

static constexpr vk::Format RGBAFormat = vk::Format::eB8G8R8A8Unorm;

static constexpr unsigned max_num_inputs = 64;

static constexpr uint32_t gmem_size =
    NSEEL_RAM_BLOCKS * NSEEL_RAM_ITEMSPERBLOCK;

struct Uniforms {
  float iResolution_w, iResolution_h;
  float iTime;
  float iSampleRate;
  float iFrameRate;
  float iWet;
  int num_inputs;
};
static_assert(sizeof(Uniforms) < 128,
              "Keep this under 128 bytes to ensure compatibility!");

union UniformsView {
  Uniforms data;
  std::array<float, sizeof(data) / sizeof(float)> values;
};

struct SpecializationData {
  unsigned gmem_size;
  int ogler_version_maj;
  int ogler_version_min;
  int ogler_version_rev;
};

struct OglerVst::Compute {
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

  static vk::raii::DescriptorSetLayout
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

  static vk::raii::DescriptorPool create_descriptor_pool(VulkanContext &ctx) {
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

  static vk::raii::DescriptorSet
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

SharedVulkan::SharedVulkan()
    : gmem_transfer_buffer(vulkan.create_buffer<float>(
          {}, gmem_size, vk::BufferUsageFlagBits::eTransferSrc,
          vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      gmem_buffer(vulkan.create_buffer<float>(
          {}, gmem_size,
          vk::BufferUsageFlagBits::eTransferDst |
              vk::BufferUsageFlagBits::eStorageBuffer,
          vk::SharingMode::eExclusive, vk::MemoryPropertyFlagBits::eDeviceLocal,
          false)) {}

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

OglerVst::OglerVst(vst::HostCallback *hostcb)
    : vst::ReaperVstPlugin<OglerVst>(hostcb), shared(get_shared_vulkan()),
      command_buffer(shared.vulkan.create_command_buffer()),
      queue(shared.vulkan.get_queue(0)), fence(shared.vulkan.create_fence()),
      sampler(shared.vulkan.create_sampler()),
      output_transfer_buffer(shared.vulkan.create_buffer<char>(
          {}, output_width * output_height * 4,
          vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      output_image(shared.vulkan.create_image(
          output_width, output_height, RGBAFormat, vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eStorage |
              vk::ImageUsageFlagBits::eTransferSrc |
              vk::ImageUsageFlagBits::eSampled)),
      output_image_view(
          shared.vulkan.create_image_view(output_image, RGBAFormat)),
      previous_image(shared.vulkan.create_image(
          output_width, output_height, RGBAFormat, vk::ImageTiling::eOptimal,
          vk::ImageUsageFlagBits::eStorage |
              vk::ImageUsageFlagBits::eTransferSrc |
              vk::ImageUsageFlagBits::eSampled)),
      previous_image_view(
          shared.vulkan.create_image_view(previous_image, RGBAFormat)),
      empty_input(create_input_image(1, 1)),
      input_resolution_buffer(
          shared.vulkan.create_buffer<std::pair<float, float>>(
              {}, max_num_inputs, vk::BufferUsageFlagBits::eUniformBuffer,
              vk::SharingMode::eExclusive,
              vk::MemoryPropertyFlagBits::eHostCoherent |
                  vk::MemoryPropertyFlagBits::eHostVisible)),
      eel_mutex(
          get_reaper_function<mutex_stub_f>("NSEEL_HOSTSTUB_EnterMutex"),
          get_reaper_function<mutex_stub_f>("NSEEL_HOSTSTUB_LEAVEMutex")) {

  one_shot_execute([&]() {
    transition_image_layout_download(command_buffer, output_image);
    transition_image_layout_download(command_buffer, previous_image);
  });

  if (auto err = recompile_shaders()) {
    DBG << *err << '\n';
  }

  auto eel_gmem_attach =
      get_reaper_function<eel_gmem_attach_f>("eel_gmem_attach");
  gmem = eel_gmem_attach("ogler", true);
}

OglerVst::~OglerVst() {
  std::unique_lock<std::mutex> lock(video_mutex);
  vproc = nullptr;
}

std::string_view OglerVst::get_effect_name() noexcept { return "ogler"; }
std::string_view OglerVst::get_vendor_name() noexcept {
  return "Francesco Bertolaccini";
}
std::string_view OglerVst::get_product_name() noexcept { return "ogler"; }
std::int32_t OglerVst::get_vendor_version() noexcept { return 1000; }

void PatchData::deserialize(std::istream &s) {
  nlohmann::json obj;
  s >> obj;

  video_shader = obj["video_shader"];

  do {
    auto &editor_data = obj["editor"];
    if (editor_data.is_null()) {
      editor_w = default_editor_w;
      editor_h = default_editor_h;
      editor_zoom = default_editor_zoom;
      break;
    }

    editor_w = editor_data["width"];
    editor_h = editor_data["height"];
    editor_zoom = editor_data["zoom"];
  } while (false);
}

void PatchData::serialize(std::ostream &s) {
  nlohmann::json obj{
      {"video_shader", video_shader},
      {
          "editor",
          {
              {"width", editor_w},
              {"height", editor_h},
              {"zoom", editor_zoom},
          },
      },
  };
  s << obj;
}

void OglerVst::save_preset_data(std::ostream &s) noexcept { data.serialize(s); }

void OglerVst::load_preset_data(std::istream &s) noexcept {
  data.deserialize(s);
  if (editor) {
    editor->reload_source();
  }
  recompile_shaders();
}

void OglerVst::save_bank_data(std::ostream &s) noexcept { save_preset_data(s); }

void OglerVst::load_bank_data(std::istream &s) noexcept { load_preset_data(s); }

std::optional<std::string> OglerVst::recompile_shaders() {
  std::unique_lock<std::mutex> video_lock(video_mutex);
  std::unique_lock<std::recursive_mutex> params_lock(params_mutex);

  auto res = compile_shader({{"<preamble>", R"(#version 460
#define OGLER_PARAMS_BINDING 0
#define OGLER_PARAMS layout(binding = OGLER_PARAMS_BINDING) uniform Params

layout (constant_id = 0) const uint ogler_gmem_size = 0;
layout (constant_id = 1) const int ogler_version_maj = 0;
layout (constant_id = 2) const int ogler_version_min = 0;
layout (constant_id = 3) const int ogler_version_rev = 0;

layout(local_size_x = 1, local_size_y = 1) in;

layout(push_constant) uniform UniformBlock {
  vec2 iResolution;
  float iTime;
  float iSampleRate;
  float iFrameRate;
  float iWet;
  int ogler_num_inputs;
};
layout(binding = 1) uniform sampler2D iChannel[];
layout(binding = 2, rgba8) uniform writeonly image2D oChannel;
layout(binding = 3) buffer readonly Gmem {
  float gmem[];
};
layout(binding = 4) uniform InputSizes {
  vec2 iChannelResolution[];
};
layout(binding = 5) uniform sampler2D ogler_previous_frame;
)"},
                             {"<source>", data.video_shader},
                             {"<epilogue>", R"(void main() {
    vec4 fragColor;
    mainImage(fragColor, vec2(gl_GlobalInvocationID));
    imageStore(oChannel, ivec2(gl_GlobalInvocationID), fragColor);
})"}},
                            /*params_binding=*/0);
  if (std::holds_alternative<std::string>(res)) {
    return std::move(std::get<std::string>(res));
  }

  auto data = std::move(std::get<ShaderData>(res));
  if (output_width != data.output_width ||
      output_height != data.output_height) {
    output_width = data.output_width;
    output_height = data.output_height;

    output_transfer_buffer = shared.vulkan.create_buffer<char>(
        {}, output_width * output_height * 4,
        vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);
    output_image = shared.vulkan.create_image(
        output_width, output_height, RGBAFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eStorage |
            vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eSampled);
    output_image_view =
        shared.vulkan.create_image_view(output_image, RGBAFormat);

    previous_image = shared.vulkan.create_image(
        output_width, output_height, RGBAFormat, vk::ImageTiling::eOptimal,
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

  int old_num = parameters.size();
  parameters.clear();
  for (auto param : data.parameters) {
    parameters.push_back(Parameter{
        .info = param,
        .value = param.default_value,
    });
  }

  try {
    compute = std::make_unique<Compute>(shared.vulkan, data.spirv_code);
  } catch (vk::Error &e) {
    return e.what();
  }
  get_effect()->numParams = parameters.size();
  if (old_num != parameters.size()) {
    this->adjust_params_num(0, -old_num);
    this->adjust_params_num(0, parameters.size());

    if (parameters.size()) {
      params_buffer = shared.vulkan.create_buffer<float>(
          {}, parameters.size(), vk::BufferUsageFlagBits::eUniformBuffer,
          vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostCoherent |
              vk::MemoryPropertyFlagBits::eHostVisible);
    } else {
      params_buffer = std::nullopt;
    }
  }

  return {};
}

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

InputImage OglerVst::create_input_image(int w, int h) {
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

IVideoFrame *
OglerVst::video_process_frame(std::span<const double> parms,
                              double project_time, double framerate,
                              vst::FrameFormat force_format) noexcept {
  std::unique_lock<std::mutex> lock(video_mutex, std::try_to_lock_t{});
  if (!lock.owns_lock()) {
    return nullptr;
  }

  if (!compute) {
    return nullptr;
  }

  output_frame =
      new_video_frame(output_width, output_height, vst::FrameFormat::RGBA);
  auto output_rowspan = output_frame->get_rowspan();
  auto output_w = output_frame->get_w();
  auto output_h = output_frame->get_h();
  assert(output_w == output_width);
  assert(output_h == output_height);
  auto num_inputs = get_video_num_inputs();

  UniformsView uniforms{
      .data =
          {
              .iResolution_w = static_cast<float>(output_width),
              .iResolution_h = static_cast<float>(output_height),
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
    std::unique_lock<EELMutex> eel_lock(eel_mutex);
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
  for (size_t i = 0; i < max_num_inputs; ++i) {
    auto input_frame = get_video_input(i, vst::FrameFormat::RGBA);
    if (!input_frame) {
      input_resolution[i] = {1, 1};
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

      if (i >= input_images.size()) {
        input_images.push_back(create_input_image(input_w, input_h));
      }

      auto &input_image = input_images[i];

      if (input_image.image.width != input_w ||
          input_image.image.height != input_h) {
        input_image = create_input_image(input_w, input_h);
      }

      input_resolution[i] = {input_w, input_h};
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

    if (params_buffer) {
      vk::DescriptorBufferInfo uniforms_info{
          .buffer = *params_buffer->buffer,
          .range = sizeof(float) * parameters.size(),
      };
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
  command_buffer.dispatch(output_width, output_height, 1);
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

void OglerVst::process(float **inputs, float **outputs,
                       std::int32_t num_samples) noexcept {
  std::copy(inputs[0], inputs[0] + num_samples, outputs[0]);
  std::copy(inputs[1], inputs[1] + num_samples, outputs[1]);
}

void OglerVst::process(double **inputs, double **outputs,
                       std::int32_t num_samples) noexcept {
  std::copy(inputs[0], inputs[0] + num_samples, outputs[0]);
  std::copy(inputs[1], inputs[1] + num_samples, outputs[1]);
}
} // namespace ogler