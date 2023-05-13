# ogler Reference Manual

ogler uses compute shaders written in GLSL 4.60. The shaders it supports are supposed to match the format used by ShaderToy as closely as possible, but there a few differences here and there.

## Entry point

ogler executes the function named `mainImage` for every pixel of the output frame. This function should have the following signature:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord)
```

`fragColor` is the color that will be stored in the pixel at coordinates `fragCoord`.

## Available inputs

Every shader has the following inputs available during processing:

| Name | Type | Description |
| ---- | ---- | ----------- |
| `ogler_version_maj` | `int` | Major version of the ogler plugin that is executing the shader |
| `ogler_version_minor` | `int` | Minor version of the ogler plugin that is executing the shader |
| `ogler_version_rev` | `int` | Revision of the ogler plugin that is executing the shader |
| `iResolution` | `vec2` | Output resolution in pixels |
| `iTime` | `float` | Time of the current frame in seconds |
| `iSampleRate` | `float` | Sample rate of the project |
| `iFrameRate` | `float` | Framerate of the project |
| `iWet` | `float` | Wet/dry percentage as set from the REAPER UI |
| `ogler_num_inputs` | `int` | Number of available input channels |
| `iChannel` | `sampler2D[]` | Input channels, up to `ogler_num_inputs` |
| `iChannelResolution` | `vec2[]` | Resolution of the input channels |
| `ogler_previous_frame` | `sampler2D` | Previous output frame |
| `gmem` | `float[]` | Access to JSFX/VideoProcessor global memory, under the `ogler` namespace |
| `ogler_gmem_size` | `uint` | Size of the accessible global memory |

## Defining input parameters

In addition to having access to other JSFX/VideoProcessor's global memory via `gmem`, ogler also allows shader to define automatable parameters.

To do so, define `float` variables inside an `OGLER_PARAMS` block at the beginning of your shader:

```glsl
OGLER_PARAMS {
    float my_param;
    float my_other_param;
}
```

> **Note**
> Only `float` parameters are currently supported, no vectors or array parameters are available.

Minimum, maximum, middle, default values and step size for every parameter can be specified by declaring constants _after the `OGLER_PARAMS` block_ in the following format:

```glsl
const float my_param_min = -10.0;
const float my_param_max = 10.0;
const float my_param_mid = 0.0;
const float my_param_def = 1.0;
const float my_param_step = 0.5;
```

Not all of the specifiers need to be present.

## Specifying the output resolution

The output resolution of the shader can be specified by declaring a constant `ogler_output_resolution` of type `ivec2`:

```glsl
const ivec2 ogler_output_resolution = ivec2(1920, 1080);
```
