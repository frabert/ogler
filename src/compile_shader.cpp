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

#include "compile_shader.hpp"

#include "string_utils.hpp"

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>

#include <sstream>
#include <stdexcept>

#define OGLER_CONCAT_(x, y) x##y
#define OGLER_CONCAT(x, y) OGLER_CONCAT_(x, y)
#define OGLER_VULKAN_TARGET                                                    \
  OGLER_CONCAT(glslang::EShTargetVulkan_, OGLER_VULKAN_VER)

namespace ogler {

static const TBuiltInResource DefaultTBuiltInResource = {
    .maxLights = 32,
    .maxClipPlanes = 6,
    .maxTextureUnits = 32,
    .maxTextureCoords = 32,
    .maxVertexAttribs = 64,
    .maxVertexUniformComponents = 4096,
    .maxVaryingFloats = 64,
    .maxVertexTextureImageUnits = 32,
    .maxCombinedTextureImageUnits = 80,
    .maxTextureImageUnits = 32,
    .maxFragmentUniformComponents = 4096,
    .maxDrawBuffers = 32,
    .maxVertexUniformVectors = 128,
    .maxVaryingVectors = 8,
    .maxFragmentUniformVectors = 16,
    .maxVertexOutputVectors = 16,
    .maxFragmentInputVectors = 15,
    .minProgramTexelOffset = -8,
    .maxProgramTexelOffset = 7,
    .maxClipDistances = 8,
    .maxComputeWorkGroupCountX = 65535,
    .maxComputeWorkGroupCountY = 65535,
    .maxComputeWorkGroupCountZ = 65535,
    .maxComputeWorkGroupSizeX = 1024,
    .maxComputeWorkGroupSizeY = 1024,
    .maxComputeWorkGroupSizeZ = 64,
    .maxComputeUniformComponents = 1024,
    .maxComputeTextureImageUnits = 16,
    .maxComputeImageUniforms = 8,
    .maxComputeAtomicCounters = 8,
    .maxComputeAtomicCounterBuffers = 1,
    .maxVaryingComponents = 60,
    .maxVertexOutputComponents = 64,
    .maxGeometryInputComponents = 64,
    .maxGeometryOutputComponents = 128,
    .maxFragmentInputComponents = 128,
    .maxImageUnits = 8,
    .maxCombinedImageUnitsAndFragmentOutputs = 8,
    .maxCombinedShaderOutputResources = 8,
    .maxImageSamples = 0,
    .maxVertexImageUniforms = 0,
    .maxTessControlImageUniforms = 0,
    .maxTessEvaluationImageUniforms = 0,
    .maxGeometryImageUniforms = 0,
    .maxFragmentImageUniforms = 8,
    .maxCombinedImageUniforms = 8,
    .maxGeometryTextureImageUnits = 16,
    .maxGeometryOutputVertices = 256,
    .maxGeometryTotalOutputComponents = 1024,
    .maxGeometryUniformComponents = 1024,
    .maxGeometryVaryingComponents = 64,
    .maxTessControlInputComponents = 128,
    .maxTessControlOutputComponents = 128,
    .maxTessControlTextureImageUnits = 16,
    .maxTessControlUniformComponents = 1024,
    .maxTessControlTotalOutputComponents = 4096,
    .maxTessEvaluationInputComponents = 128,
    .maxTessEvaluationOutputComponents = 128,
    .maxTessEvaluationTextureImageUnits = 16,
    .maxTessEvaluationUniformComponents = 1024,
    .maxTessPatchComponents = 120,
    .maxPatchVertices = 32,
    .maxTessGenLevel = 64,
    .maxViewports = 16,
    .maxVertexAtomicCounters = 0,
    .maxTessControlAtomicCounters = 0,
    .maxTessEvaluationAtomicCounters = 0,
    .maxGeometryAtomicCounters = 0,
    .maxFragmentAtomicCounters = 8,
    .maxCombinedAtomicCounters = 8,
    .maxAtomicCounterBindings = 1,
    .maxVertexAtomicCounterBuffers = 0,
    .maxTessControlAtomicCounterBuffers = 0,
    .maxTessEvaluationAtomicCounterBuffers = 0,
    .maxGeometryAtomicCounterBuffers = 0,
    .maxFragmentAtomicCounterBuffers = 1,
    .maxCombinedAtomicCounterBuffers = 1,
    .maxAtomicCounterBufferSize = 16384,
    .maxTransformFeedbackBuffers = 4,
    .maxTransformFeedbackInterleavedComponents = 64,
    .maxCullDistances = 8,
    .maxCombinedClipAndCullDistances = 8,
    .maxSamples = 4,
    .maxMeshOutputVerticesNV = 256,
    .maxMeshOutputPrimitivesNV = 512,
    .maxMeshWorkGroupSizeX_NV = 32,
    .maxMeshWorkGroupSizeY_NV = 1,
    .maxMeshWorkGroupSizeZ_NV = 1,
    .maxTaskWorkGroupSizeX_NV = 32,
    .maxTaskWorkGroupSizeY_NV = 1,
    .maxTaskWorkGroupSizeZ_NV = 1,
    .maxMeshViewCountNV = 4,
    .maxMeshOutputVerticesEXT = 256,
    .maxMeshOutputPrimitivesEXT = 256,
    .maxMeshWorkGroupSizeX_EXT = 128,
    .maxMeshWorkGroupSizeY_EXT = 128,
    .maxMeshWorkGroupSizeZ_EXT = 128,
    .maxTaskWorkGroupSizeX_EXT = 128,
    .maxTaskWorkGroupSizeY_EXT = 128,
    .maxTaskWorkGroupSizeZ_EXT = 128,
    .maxMeshViewCountEXT = 4,
    .maxDualSourceDrawBuffersEXT = 1,

    .limits = {
        .nonInductiveForLoops = 1,
        .whileLoops = 1,
        .doWhileLoops = 1,
        .generalUniformIndexing = 1,
        .generalAttributeMatrixVectorIndexing = 1,
        .generalVaryingIndexing = 1,
        .generalSamplerIndexing = 1,
        .generalVariableIndexing = 1,
        .generalConstantMatrixVectorIndexing = 1,
    }};

class ParamCollector : public glslang::TIntermTraverser {
  std::vector<ParameterInfo> &params;
  std::optional<int> &output_width;
  std::optional<int> &output_height;
  int params_binding;

  ParameterInfo *find_param(const std::string &name) {
    for (auto &param : params) {
      if (param.name == name) {
        return &param;
      }
    }
    return nullptr;
  }

  static std::string remove_suffix(const glslang::TString &str,
                                   const std::string &suffix) {
    return std::string(str.substr(0, str.size() - suffix.size()));
  }

public:
  ParamCollector(ShaderData &data, int params_binding)
      : params(data.parameters), output_width(data.output_width),
        output_height(data.output_height), params_binding(params_binding) {}

  void visitSymbol(glslang::TIntermSymbol *sym) final {
    auto &type = sym->getType();
    auto &c = sym->getConstArray();
    bool isArray = sym->isArray();
    bool isVector = sym->isVector();
    if (sym->getBasicType() == glslang::EbtBlock &&
        type.getQualifier().layoutBinding == params_binding) {
      for (auto &field : *type.getStruct()) {
        auto ftype = field.type->getBasicType();
        auto &fname = field.type->getFieldName();
        if (ftype != glslang::EbtFloat && field.type->isArray() ||
            field.type->isVector()) {
          std::stringstream errmsg;
          errmsg << "ERROR: " << field.loc.getStringNameOrNum(false) << ':'
                 << field.loc.line
                 << ": only parameters of type float are accepted, field `"
                 << fname << "' has type " << field.type->getBasicTypeString();
          throw std::runtime_error(errmsg.str());
        }

        params.push_back(ParameterInfo{
            .name = fname.c_str(),
            .display_name = fname.c_str(),
            .default_value = 0.5f,
            .minimum_val = 0.0f,
            .maximum_val = 1.0f,
            .middle_value = 0.5f,
            .step_size = 0.0f,
        });
      }
    } else if (!isArray && sym->getBasicType() == glslang::EbtFloat &&
               c.size() == 1) {
      auto &name = sym->getName();
      if (name.ends_with("_min")) {
        auto param_name = remove_suffix(name, "_min");
        auto param = find_param(param_name);
        if (param) {
          param->minimum_val = c[0].getDConst();
        }
      } else if (name.ends_with("_max")) {
        auto param_name = remove_suffix(name, "_max");
        auto param = find_param(param_name);
        if (param) {
          param->maximum_val = c[0].getDConst();
        }
      } else if (name.ends_with("_mid")) {
        auto param_name = remove_suffix(name, "_mid");
        auto param = find_param(param_name);
        if (param) {
          param->middle_value = c[0].getDConst();
        }
      } else if (name.ends_with("_def")) {
        auto param_name = remove_suffix(name, "_def");
        auto param = find_param(param_name);
        if (param) {
          param->default_value = c[0].getDConst();
        }
      } else if (name.ends_with("_step")) {
        auto param_name = remove_suffix(name, "_step");
        auto param = find_param(param_name);
        if (param) {
          param->step_size = c[0].getDConst();
        }
      }
    } else if (isVector && sym->getBasicType() == glslang::EbtInt &&
               c.size() == 2) {
      auto &name = sym->getName();
      if (name == "ogler_output_resolution") {
        output_width = c[0].getIConst();
        output_height = c[1].getIConst();
      }
    }
  }

  bool visitBinary(glslang::TVisit, glslang::TIntermBinary *) final {
    return false;
  }

  bool visitUnary(glslang::TVisit, glslang::TIntermUnary *) final {
    return false;
  }

  bool visitSelection(glslang::TVisit, glslang::TIntermSelection *) final {
    return false;
  }

  bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate *agg) final {
    auto op = agg->getOp();
    return op == glslang::EOpLinkerObjects || op == glslang::EOpSequence;
  }

  bool visitLoop(glslang::TVisit, glslang::TIntermLoop *) final {
    return false;
  }

  bool visitBranch(glslang::TVisit, glslang::TIntermBranch *) final {
    return false;
  }

  bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch *) final {
    return false;
  }
};

std::variant<ShaderData, std::string>
compile_shader(const std::vector<std::pair<std::string, std::string>> &source,
               int params_binding) {

  glslang::TShader shader(EShLangCompute);
  std::vector<const char *> sources;
  std::vector<const char *> names;
  for (auto &[name, contents] : source) {
    sources.push_back(contents.c_str());
    names.push_back(name.c_str());
  }

  shader.setStringsWithLengthsAndNames(sources.data(), nullptr, names.data(),
                                       sources.size());
  shader.setEnvInput(glslang::EShSourceGlsl, EShLangCompute,
                     glslang::EShClientVulkan, 100);
  shader.setEnvClient(glslang::EShClientVulkan, OGLER_VULKAN_TARGET);
  shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv,
                      glslang::EShTargetLanguageVersion::EShTargetSpv_1_0);
  if (!shader.parse(&DefaultTBuiltInResource, 110, true,
                    EShMessages::EShMsgDefault)) {
    return std::string(shader.getInfoLog());
  }

  glslang::TProgram prog;
  prog.addShader(&shader);
  if (!prog.link(EShMessages::EShMsgDefault)) {
    return std::string(prog.getInfoLog());
  }

  ShaderData data;
  ParamCollector collector(data, params_binding);
  auto iterm = prog.getIntermediate(EShLangCompute);
  try {
    iterm->getTreeRoot()->traverse(&collector);
  } catch (std::runtime_error &e) {
    return e.what();
  }
  glslang::GlslangToSpv(*iterm, data.spirv_code);
  return data;
}

sciter::value ParameterInfo::to_json() const {
  return sciter::value::make_map({
      {"name", name},
      {"display_name", display_name},
      {"default_value", default_value},
      {"minimum_val", minimum_val},
      {"maximum_val", maximum_val},
      {"middle_value", middle_value},
      {"step_size", step_size},
  });
}

void ParameterInfo::from_json(sciter::value value) {
  name = to_string(value.get_item("name").get(L""));
  display_name = to_string(value.get_item("display_name").get(L""));
  default_value = value.get_item("default_value").get(.5f);
  minimum_val = value.get_item("minimum_val").get(0.f);
  maximum_val = value.get_item("maximum_val").get(1.f);
  middle_value = value.get_item("middle_value").get(.5f);
  step_size = value.get_item("step_size").get(0.f);
}

sciter::value Parameter::to_json() const {
  return sciter::value::make_map({
      {"info", info.to_json()},
      {"value", value},
  });
}

void Parameter::from_json(sciter::value value) {
  if (value.get_item("info").is_nothing()) {
    info = {};
  } else {
    info.from_json(value.get_item("info"));
  }
  this->value = value.get_item("value").get(info.default_value);
}
} // namespace ogler