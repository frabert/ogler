# Ogler

Ogler is a CLAP plugin for REAPER that allows writing video (and audio?) effects using GLSL, similar to what happens in ShaderToy.

## How do I write shaders?

If you have previous experience with writing ShaderToys or shaders in general, see the [Reference Manual](/docs/Reference.md).

If you need an introduction to shaders, see the [Tutorial](/docs/Tutorial.md).

## Compiling

You will need LunarG's Vulkan SDK and re2c installed and reachable by CMake. Both can be installed using Chocolatey, but any other method will work as long you take care to update the relevant environment variables:

    choco install vulkan-sdk re2c

### Obtaining Sciter

Ogler uses [Sciter](https://sciter.com/) for its GUI. Sciter is not free (as in freedom) software, but a free (as in beer) version is available with the caveat that it's only available as a binary, dynamically linked library.

If you compile ogler using this dynamically linked version of Sciter, you will need to put `sciter.dll` beside REAPER's binary in order to use ogler.

### Using vcpkg

Vcpkg is the only officially supported way of compiling ogler.

    git clone https://github.com/frabert/ogler
    cmake --toolchain $PATH_TO_VCPKG/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-dynamic-sciter -S ogler -B build-ogler
    cmake --build build-ogler

## System requirements

You'll need modern graphics drivers.

And by modern I mean they need to support Vulkan 1.0, so not _that_ modern, but still.

## Licensing

Ogler is released under the terms of the GPLv3 license. A special exception is made for linking against Sciter: you are allowed to freely distribute copies of ogler's source code and binary artifacts without having to also distribute Sciter's source code, whether you linked ogler against Sciter statically or dynamically.
