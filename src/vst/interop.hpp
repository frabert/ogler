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

#include <array>
#include <cstdint>

namespace vst {
enum class PluginOpcode : std::int32_t {
  Open = 0,
  Close,
  SetProgram,
  GetProgram,
  SetProgramName,
  GetProgramName,
  GetParamLabel,
  GetParamDisplay,
  GetParamName,
  GetVu,
  SetSampleRate,
  SetBlockSize,
  MainsChanged,
  EditGetRect,
  EditOpen,
  EditClose,
  EditDraw,
  EditMouse,
  EditKey,
  EditIdle,
  EditTop,
  EditSleep,
  Identify,
  GetChunk,
  SetChunk,
  ProcessEvents,
  CanBeAutomated,
  String2Parameter,
  GetNumProgramCategories,
  GetProgramNameIndexed,
  CopyProgram,
  ConnectInput,
  ConnectOutput,
  GetInputProperties,
  GetOutputProperties,
  GetPlugCategory,
  GetCurrentPosition,
  GetDestinationBuffer,
  OfflineNotify,
  OfflinePrepare,
  OfflineRun,
  ProcessVarIO,
  SetSpeakerArrangement,
  SetBlockSizeAndSampleRate,
  SetBypass,
  GetEffectName,
  GetErrorText,
  GetVendorString,
  GetProductString,
  GetVendorVersion,
  VendorSpecific,
  CanDo,
  GetTailSize,
  Idle,
  GetIcon,
  SetViewPosition,
  GetParameterProperties,
  KeysRequired,
  GetVstVersion,
  EditKeyDown,
  EditKeyUp,
  SetEditKnobMode,
  GetMidiProgramName,
  GetMidiProgramCategory,
  HasMidiProgramsChanged,
  GetMidiKeyName,
  BeginSetProgram,
  EndSetProgram,
  GetSpeakerArrangement,
  ShellGetNextPlugin,
  StartProcess,
  StopProcess,
  SetTotalSampleToProcess,
  SetPanLaw,
  BeginLoadBank,
  BeginLoadProgram,
  SetProcessPrecision,
  GetNumMidiInputChannels,
  GetNumMidiOutputChannels
};

enum class HostOpcodes : std::int32_t {

  Automate = 0,
  Version,
  CurrentId,
  Idle,
  PinConnected [[deprecated]],
  WantMidi [[deprecated]] = 6,
  GetTime,
  ProcessEvents,
  SetTime [[deprecated]],
  TempoAt [[deprecated]],
  GetNumAutomatableParams [[deprecated]],
  GetParameterQuantization [[deprecated]],

  IOChanged,

  NeedIdle [[deprecated]],
  SizeWindow,
  GetSampleRate,
  GetBlockSize,
  GetInputLatency,
  GetOutputLatency,

  GetPreviousPlug [[deprecated]],
  GetNextPlug [[deprecated]],
  WillReplaceOrAccumulate [[deprecated]],

  GetCurrentProcessLevel,
  GetAutomationState,

  OfflineStart,
  OfflineRead,
  OfflineWrite,
  OfflineGetCurrentPass,
  OfflineGetCurrentMetaPass,

  SetOutputSampleRate [[deprecated]],
  GetOutputSpeakerArrangement [[deprecated]],

  GetVendorString,
  GetProductString,
  GetVendorVersion,
  VendorSpecific,

  SetIcon [[deprecated]],

  CanDo,
  GetLanguage,

  OpenWindow [[deprecated]],
  CloseWindow [[deprecated]],

  GetDirectory,
  UpdateDisplay,
  BeginEdit,

  EndEdit,

  OpenFileSelector,
  CloseFileSelector,

  EditFile [[deprecated]],

  GetChunkFile [[deprecated]],

  GetInputSpeakerArrangement [[deprecated]]
};

enum AEffectFlags {
  EffectHasEditor = 1 << 0,
  EffectCanReplacing = 1 << 4,
  EffectProgramChunks = 1 << 5,
  EffectIsSynth = 1 << 8,
  EffectNoSoundInStop = 1 << 9,
  EffectCanDoubleReplacing = 1 << 12
};

struct AEffect;

using MasterCallback = std::intptr_t(AEffect *effect, PluginOpcode opcode,
                                     std::int32_t index, std::intptr_t value,
                                     void *ptr, float opt);
using HostCallback = std::intptr_t(AEffect *effect, HostOpcodes opcode,
                                   std::int32_t index, std::intptr_t value,
                                   void *ptr, float opt);
using ProcessProc = void(AEffect *effect, float **inputs, float **outputs,
                         std::int32_t sampleFrames);
using ProcessDoubleProc = void(AEffect *effect, double **inputs,
                               double **outputs, std::int32_t sampleFrames);
using SetParameterProc = void(AEffect *effect, std::int32_t index,
                              float parameter);
using GetParameterProc = float(AEffect *effect, std::int32_t index);

constexpr int MaxProgNameLen = 24;
constexpr int MaxParamStrLen = 8;
constexpr int MaxVendorStrLen = 64;
constexpr int MaxProductStrLen = 64;
constexpr int MaxEffectNameLen = 32;
constexpr int MaxNameLen = 64;
constexpr int MaxLabelLen = 64;
constexpr int MaxShortLabelLen = 8;
constexpr int MaxCategLabelLen = 24;
constexpr int MaxFileNameLen = 100;

constexpr int Magic = 'VstP';

struct Rectangle {
  std::int16_t top, left, bottom, right;
};

enum class Supported : std::int32_t { No = -1, Maybe = 0, Yes = 0 };

enum class PluginCategory : std::int32_t {
  Unknown = 0,
  Effect,
  Synth,
  Analysis,
  Mastering,
  Spacializer,
  RoomFx,
  SurroundEffect,
  Restoration,
  OfflineProcess,
  Shell,
  Generator
};

enum ParameterFlags {
  ParameterIsSwitch = 1 << 0,
  ParameterUsesIntegerMinMax = 1 << 1,
  ParameterUsesFloatStep = 1 << 2,
  ParameterUsesIntStep = 1 << 3,
  ParameterSupportsDisplayIndex = 1 << 4,
  ParameterSupportsDisplayCategory = 1 << 5,
  ParameterCanRamp = 1 << 6
};

struct ParameterProperties {
  float stepFloat;
  float smallStepFloat;
  float largeStepFloat;
  std::array<char, MaxLabelLen> label;
  ParameterFlags flags;
  int32_t minInteger;
  int32_t maxInteger;
  int32_t stepInteger;
  int32_t largeStepInteger;
  std::array<char, MaxShortLabelLen> shortLabel;
  int16_t displayIndex;
  int16_t category;
  int16_t numParametersInCategory;
  int16_t reserved;
  std::array<char, MaxCategLabelLen> categoryLabel;
  std::array<char, 16> future;
};

struct AEffect {
  std::int32_t magic = Magic;
  MasterCallback *dispatcher;
  [[deprecated]] ProcessProc *process;
  SetParameterProc *setParameter;
  GetParameterProc *getParameter;

  std::int32_t numPrograms;
  std::int32_t numParams;
  std::int32_t numInputs;
  std::int32_t numOutputs;

  std::int32_t flags;

  std::intptr_t resvd1, resvd2;

  std::int32_t initialDelay;

  [[deprecated]] std::int32_t realQualities;
  [[deprecated]] std::int32_t offQualities;
  [[deprecated]] float ioRatio;

  void *object;
  void *user;

  std::int32_t uniqueID;
  std::int32_t version;

  ProcessProc *processReplacing;
  ProcessDoubleProc *processDoubleReplacing;

  uint8_t future[56];
};

} // namespace vst