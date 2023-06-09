# Ogler

Ogler is a CLAP plugin for REAPER that allows writing video (and audio?) effects using GLSL, similar to what happens in ShaderToy.

## How do I write shaders?

If you have previous experience with writing ShaderToys or shaders in general, see the [Reference Manual](/docs/Reference.md).

If you need an introduction to shaders, see the [Tutorial](/docs/Tutorial.md).

## Compiling

You will need LunarG's Vulkan SDK installed and reachable by CMake.

Use vcpkg for the smoothest experience:

    git clone https://github.com/frabert/ogler
    cmake --toolchain $PATH_TO_VCPKG/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -S ogler -B build-ogler
    cmake --build build-ogler

vcpkg and cmake should take care of all the dependencies by themselves.

## System requirements

You'll need modern graphics drivers.

And by modern I mean they need to support Vulkan 1.0, so not _that_ modern, but still.
