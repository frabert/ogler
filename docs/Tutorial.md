# Getting started writing video shaders

ogler shaders are written in a language called GLSL. This tutorial will assume that the reader already has experience with the GLSL language. Resources on the language itself can easily be found on the net.

Any knowledge on how ShaderToy works can largely be reused for ogler shaders, as ogler makes an effort to be as close to ShaderToy as possible.

## What is a video shader?

A video shader is a program that is executed on a video frame, and produces a new frame as output. For those familiar with 3D programming, it's essentially equivalent to a fragment shader (pixel shader in DirectX parlance). This is similar to what happens when calling `gfx_evalrect` in JSFX.

Conceptually, this is what happens:

```pseudocode
for each pixel p in the output frame:
    color(p) = shader(coords(p))
```

`shader` is what we are going to define using our ogler shaders. Note how each pixel is only produced once, i.e. once the color of a pixel is decided, it cannot be changed.

## Writing our first shader

We are going to write a shader that crossfades between two input channels. Let's start by defining our main function:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoords) {
```

`fragColor` is the color of the output pixel in the frame. Any value present in this parameter at the exit from the `mainImage` function will be applied to the output frame at the coordinates specified in `fragCoord`.

### Uniform coordinates

Input channels are given to shaders under the form of samplers: this means that accessing the contents of each input can be done through uniform coordinates.

Uniform coordinates are floating point values that range from 0 to 1. The point (0, 0) represents the topmost right corner of an image, while (1, 1) represents the bottom left corner. (0.5, 0.5) is the middle point. The two components of a point expressed in uniform coordinates are typically called `u` and `v`.

Converting from pixel coordinates to uniform coordinates is just a matter of dividing the pixel coordinates by the size of the image we're sampling:

```glsl
    vec2 uv = fragCoords / iResolution;
```

### Sampling input channels

Once we have our uniform coordinates, getting the color of the image at that point is done using the [`texture`](https://docs.gl/sl4/texture) function:

```glsl
    vec4 chan0_color = texture(iChannel[0], uv);
    vec4 chan1_color = texture(iChannel[1], uv);
```

Input channels are available in the `iChannel` array. Channels beyond the last available one are black.

### Mixing the channels

Now that we have the color of our two channels, we can mix them using the aptly named [`mix`](https://docs.gl/sl4/mix) GLSL function:

```glsl
    fragColor = mix(chan0_color, chan1_color, 0.5);
}
```

The third parameter of the `mix` function specifies the proportion in which the two components will be mixed: a value of 0 means the output value is equal to the first parameter, a value of 1 means the output value is equal to the second parameter. In our case, the value of 0.5 means that an equal amount of both parameters is used for the output value.

Our shader should now look something like this:

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoords) {
    vec2 uv = fragCoords / iResolution;
    vec4 chan0_color = texture(iChannel[0], uv);
    vec4 chan1_color = texture(iChannel[1], uv);
    fragColor = mix(chan0_color, chan1_color, 0.5);
}
```

### Making the mix value automatable

What if we wanted to make it possible for the user to specify and automate the mixing proportion? This is possible by declaring a new parameter in a `OGLER_PARAMS` section at the top of the shader:

```glsl
OGLER_PARAMS {
    float mix_proportion;
}
```

and then changing the last row in our main function to

```glsl
    fragColor = mix(chan0_color, chan1_color, mix_proportion);
```

The shader should now look like this:

```glsl
OGLER_PARAMS {
    float mix_proportion;
}

void mainImage(out vec4 fragColor, in vec2 fragCoords) {
    vec2 uv = fragCoords / iResolution;
    vec4 chan0_color = texture(iChannel[0], uv);
    vec4 chan1_color = texture(iChannel[1], uv);
    fragColor = mix(chan0_color, chan1_color, mix_proportion);
}
```

After recompilation, REAPER should now offer the option to change the value of `mix_proportion` by switching the UI view of the ogler plugin, or by using the parameter lane in the track view.
