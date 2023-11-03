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
#include "compile_shader.hpp"
#include "ogler_editor.hpp"
#include "sciter_scintilla.hpp"

#include <clap/events.h>
#include <clap/ext/audio-ports.h>
#include <clap/ext/params.h>
#include <clap/process.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <reaper_plugin_functions.h>

#include <algorithm>
#include <optional>
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

int Ogler::get_output_width() {
  if (shader_output_width.has_value()) {
    return *shader_output_width;
  } else {
    auto [w, h] = reaper->get_current_project_size(fallback_output_width,
                                                   fallback_output_height);
    return w;
  }
}

int Ogler::get_output_height() {
  if (shader_output_height.has_value()) {
    return *shader_output_height;
  } else {
    auto [w, h] = reaper->get_current_project_size(fallback_output_width,
                                                   fallback_output_height);
    return h;
  }
}

static ScintillaEditorFactory scintilla_factory(get_hinstance());

Ogler::Ogler(const clap::host &host)
    : host(host), reaper(IReaper::get_reaper(host)),
      shared(get_shared_vulkan()),
      command_buffer(shared.vulkan.create_command_buffer()),
      queue(shared.vulkan.get_queue(0)), fence(shared.vulkan.create_fence()),
      sampler(shared.vulkan.create_sampler()),
      output_transfer_buffer(shared.vulkan.create_buffer<char>(
          {}, get_output_width() * get_output_height() * 4,
          vk::BufferUsageFlagBits::eTransferDst, vk::SharingMode::eExclusive,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent)),
      output_image(
          shared.vulkan.create_image(get_output_width(), get_output_height(),
                                     RGBAFormat, vk::ImageTiling::eOptimal,
                                     vk::ImageUsageFlagBits::eStorage |
                                         vk::ImageUsageFlagBits::eTransferSrc |
                                         vk::ImageUsageFlagBits::eSampled)),
      output_image_view(
          shared.vulkan.create_image_view(output_image, RGBAFormat)),
      previous_image(
          shared.vulkan.create_image(get_output_width(), get_output_height(),
                                     RGBAFormat, vk::ImageTiling::eOptimal,
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
                  vk::MemoryPropertyFlagBits::eHostVisible)) {}

Ogler::~Ogler() {
  std::unique_lock<std::mutex> lock(video_mutex);
  vproc = nullptr;
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

bool Ogler::activate(double sample_rate, uint32_t min_frames_count,
                     uint32_t max_frames_count) {
  compiler_error = recompile_shaders();

  if (!compiler_error.has_value()) {
    if (editor) {
      editor->params_changed(data.parameters);
    }
    vproc = reaper->create_video_processor();
    vproc->userdata = this;
    vproc->process_frame =
        [](IREAPERVideoProcessor *vproc, const double *parmlist, int nparms,
           double project_time, double frate, int force_format) {
          auto plugin = static_cast<Ogler *>(vproc->userdata);
          return plugin->video_process_frame(
              std::span{parmlist, static_cast<size_t>(nparms)}, project_time,
              frate, static_cast<FrameFormat>(force_format));
        };
    vproc->get_parameter_value = [](IREAPERVideoProcessor *vproc, int idx,
                                    double *valueOut) -> bool {
      auto plugin = static_cast<Ogler *>(vproc->userdata);
      auto val = plugin->params_get_value(static_cast<clap_id>(idx));
      if (val.has_value()) {
        *valueOut = *val;
        return true;
      } else {
        return false;
      }
    };
  } else if (editor) {
    editor->compiler_error(*compiler_error);
  }
  return true;
}
void Ogler::deactivate() {
  std::unique_lock<std::mutex> lock(video_mutex);
  vproc = nullptr;
}

bool Ogler::start_processing() { return true; }

void Ogler::stop_processing() {}

void Ogler::reset() {}

clap_process_status Ogler::process(const clap_process_t &process) {
  handle_events(*process.in_events);
  if (process.audio_inputs->data64) {
    std::copy_n(process.audio_inputs->data64[0], process.frames_count,
                process.audio_outputs->data64[0]);
    std::copy_n(process.audio_inputs->data64[1], process.frames_count,
                process.audio_outputs->data64[1]);
  } else {
    std::copy_n(process.audio_inputs->data32[0], process.frames_count,
                process.audio_outputs->data32[0]);
    std::copy_n(process.audio_inputs->data32[1], process.frames_count,
                process.audio_outputs->data32[1]);
  }
  return CLAP_PROCESS_CONTINUE;
}

void *Ogler::get_extension(std::string_view id) { return nullptr; }

void Ogler::on_main_thread() {}

void to_json(nlohmann::json &j, const Parameter &p) {
  j = {
      {"info", p.info},
      {"value", p.value},
  };
}

void from_json(const nlohmann::json &j, Parameter &p) {
  j.at("info").get_to(p.info);
  j.at("value").get_to(p.value);
}

void PatchData::deserialize(std::istream &s) {
  nlohmann::json obj;
  s >> obj;

  video_shader = obj.at("video_shader");

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

  try {
    obj.at("parameters").get_to(parameters);
  } catch (const nlohmann::json::out_of_range &) {
  }
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
      {"parameters", parameters},
  };
  s << obj;
}

bool Ogler::state_save(std::ostream &s) {
  data.serialize(s);
  return true;
}

bool Ogler::state_load(std::istream &s) {
  data.deserialize(s);
  if (editor) {
    editor->reload_source();
  }
  host.request_restart();
  return true;
}

std::optional<std::string> Ogler::recompile_shaders() {
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

  auto shader_data = std::move(std::get<ShaderData>(res));

  size_t old_num = data.parameters.size();
  data.parameters.resize(shader_data.parameters.size());
  for (size_t i = 0; i < shader_data.parameters.size(); ++i) {
    auto &param = shader_data.parameters[i];
    data.parameters[i].info = param;
    if (i >= old_num) {
      data.parameters[i].value = param.default_value;
    }
  }

  try {
    compute = std::make_unique<Compute>(shared.vulkan, shader_data.spirv_code);
  } catch (vk::Error &e) {
    return e.what();
  }

  if (data.parameters.size()) {
    params_buffer = shared.vulkan.create_buffer<float>(
        {}, data.parameters.size(), vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive,
        vk::MemoryPropertyFlagBits::eHostCoherent |
            vk::MemoryPropertyFlagBits::eHostVisible);
  } else {
    params_buffer = std::nullopt;
  }

  host.params_rescan(CLAP_PARAM_RESCAN_ALL);
  return std::nullopt;
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

      if (i >= input_images.size()) {
        input_images.push_back(create_input_image(input_w, input_h));
      }

      auto &input_image = input_images[i];

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

uint32_t Ogler::audio_ports_count(bool is_input) { return 1; }

std::optional<clap_audio_port_info_t> Ogler::audio_ports_get(uint32_t index,
                                                             bool is_input) {
  if (index >= 1) {
    return std::nullopt;
  }
  return {{
      .id = 0,
      .flags = CLAP_AUDIO_PORT_IS_MAIN | CLAP_AUDIO_PORT_SUPPORTS_64BITS,
      .channel_count = 2,
      .port_type = CLAP_PORT_STEREO,
      .in_place_pair = 0,
  }};
}

bool Ogler::gui_is_api_supported(std::string_view api, bool is_floating) {
  return api == CLAP_WINDOW_API_WIN32;
}

std::optional<std::pair<const char *, bool>> Ogler::gui_get_preferred_api() {
  return {{CLAP_WINDOW_API_WIN32, false}};
}

bool Ogler::gui_create(std::string_view api, bool is_floating) {
  if (api != CLAP_WINDOW_API_WIN32) {
    return false;
  }

  return true;
}

void Ogler::gui_destroy() { editor = nullptr; }

bool Ogler::gui_set_scale(double scale) { return false; }

std::optional<std::pair<uint32_t, uint32_t>> Ogler::gui_get_size() {
  return {{data.editor_w, data.editor_h}};
}

bool Ogler::gui_can_resize() { return true; }

std::optional<clap_gui_resize_hints_t> Ogler::gui_get_resize_hints() {
  return {{
      .can_resize_horizontally = true,
      .can_resize_vertically = true,
      .preserve_aspect_ratio = false,
  }};
}

bool Ogler::gui_adjust_size(uint32_t &width, uint32_t &height) { return true; }

bool Ogler::gui_set_size(uint32_t width, uint32_t height) {
  if (!editor) {
    return true;
  }
  SetWindowPos(editor->hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE);
  data.editor_w = width;
  data.editor_h = height;
  return true;
}

class OglerEditorInterface final : public EditorInterface {
  Ogler &plugin;

public:
  OglerEditorInterface(Ogler &plugin) : plugin(plugin) {}

  void recompile_shaders() final { plugin.host.request_restart(); }

  void set_shader_source(const std::string &source) final {
    plugin.data.video_shader = source;
    plugin.host.state_mark_dirty();
  }

  const std::string &get_shader_source() final {
    return plugin.data.video_shader;
  }
  int get_zoom() final { return plugin.data.editor_zoom; }

  void set_zoom(int zoom) final {
    plugin.data.editor_zoom = zoom;
    plugin.host.state_mark_dirty();
  }

  int get_width() final { return plugin.data.editor_w; }
  int get_height() final { return plugin.data.editor_h; }

  void set_width(int w) final {
    plugin.data.editor_w = w;
    plugin.host.state_mark_dirty();
  }

  void set_height(int h) final {
    plugin.data.editor_h = h;
    plugin.host.state_mark_dirty();
  }

  void set_parameter(size_t index, float value) final {
    std::unique_lock<std::mutex> lock(plugin.video_mutex);
    plugin.data.parameters[index].value = value;
    plugin.host.params_rescan(CLAP_PARAM_RESCAN_VALUES);
  }
};

bool Ogler::gui_set_parent(const clap_window_t &window) {
  editor =
      std::make_unique<Editor>(static_cast<HWND>(window.win32), get_hinstance(),
                               std::make_unique<OglerEditorInterface>(*this));
  if (compiler_error) {
    editor->compiler_error(*compiler_error);
  } else {
    editor->params_changed(data.parameters);
  }
  return true;
}

bool Ogler::gui_set_transient(const clap_window_t &window) { return false; }

void Ogler::gui_suggest_title(std::string_view title) {
  SetWindowText(editor->hwnd, title.data());
}

bool Ogler::gui_show() { return true; }

bool Ogler::gui_hide() { return true; }
} // namespace ogler