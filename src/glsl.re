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

/*!re2c

mcm = "/*" ([^*] | ("*" [^/]))* "*""/";
scm = "//" [^\n]* "\n";

vec_type = [iubd]? "vec" [234];

mat_size =
    "2" ( "x2" | "x3" | "x4" )?
    | "3" ( "x2" | "x3" | "x4" )?
    | "4" ( "x2" | "x3" | "x4" )?;

mat_type = [iubd]? "mat" mat_size;

ws = [ \t\v\n\r];

digit_nonzero = [1-9];
digit = [0-9];
oct = [0-7];
hex = [a-fA-F0-9];
alpha = [a-zA-Z_];

dimension = "1D" | "2D" | "3D" | "Cube";

sampler_type_suffix = ("MS"? "Array" | "Rect") "Shadow"?;
texture_type_suffix = ("MS"? "Array" | "Rect");

type =
    "void"
    | "bool"
    | "uint"
    | "int"
    | "float"
    | "double"
    | vec_type
    | mat_type
    | [iu]? "sampler" dimension sampler_type_suffix?
    | [iu]? "texture" dimension texture_type_suffix?
    | [iu]? "image" dimension texture_type_suffix?
    | "atomic_uint"
    | "sampler"
    | "samplerShadow";

bool_lit = "true" | "false";

keyword =
    "const"
    | "uniform"
    | "buffer"
    | "shared"
    | "attribute"
    | "varying"
    | "coherent"
    | "volatile"
    | "restrict"
    | "readonly"
    | "writeonly"
    | "centroid"
    | "flat"
    | "smooth"
    | "nonperspective"
    | "patch"
    | "sample"
    | "invariant"
    | "precise"
    | "break"
    | "continue"
    | "do"
    | "for"
    | "while"
    | "switch"
    | "case"
    | "default"
    | "if"
    | "else"
    | "subroutine"
    | "in"
    | "out"
    | "inout"
    | "discard"
    | "return"
    | "lowp"
    | "mediump"
    | "highp"
    | "precision"
    | "struct"
    | "common"
    | "partition"
    | "active"
    | "asm"
    | "class"
    | "union"
    | "enum"
    | "typedef"
    | "template"
    | "this"
    | "resource"
    | "goto"
    | "inline"
    | "noinline"
    | "public"
    | "static"
    | "extern"
    | "external"
    | "interface"
    | "long"
    | "short"
    | "half"
    | "fixed"
    | "unsigned"
    | "superp"
    | "input"
    | "output"
    | "hvec2"
    | "hvec3"
    | "hvec4"
    | "fvec2"
    | "fvec3"
    | "fvec4"
    | "filter"
    | "sizeof"
    | "cast"
    | "namespace"
    | "using"
    | "sampler3DRect";

ident = alpha (alpha | digit)*;

int_lit = ('0' oct+ | digit_nonzero digit* | '0x' hex+) 'u';

float_lit_suffix = 'l' | 'f' | "lf" | "LF";
fract_constant =
    digit+ "." digit+
    | digit+ "."
    | "." digit+;
exp_part = 'e' ("+" | "-")? digit+;
float_lit = (fract_constant exp_part? | digit+ exp_part) float_lit_suffix?;

punct =
    "."
    | "+"
    | "-"
    | "/"
    | "*"
    | "%"
    | "<"
    | ">"
    | "["
    | "]"
    | "("
    | ")"
    | "{"
    | "}"
    | "^"
    | "|"
    | "&"
    | "~"
    | "="
    | "!"
    | ":"
    | ";"
    | ","
    | "?";

builtins =
    "abs"
    | "acos"
    | "acosh"
    | "all"
    | "any"
    | "asin"
    | "asinh"
    | "atan"
    | "atanh"
    | "atomicAdd"
    | "atomicAnd"
    | "atomicCompSwap"
    | "atomicCounter"
    | "atomicCounterDecrement"
    | "atomicCounterIncrement"
    | "atomicExchange"
    | "atomicMax"
    | "atomicMin"
    | "atomicOr"
    | "atomicXor"
    | "barrier"
    | "bitCount"
    | "bitfieldExtract"
    | "bitfieldInsert"
    | "bitfieldReverse"
    | "ceil"
    | "clamp"
    | "cos"
    | "cosh"
    | "cross"
    | "degrees"
    | "determinant"
    | "dFdx"
    | "dFdxCoarse"
    | "dFdxFine"
    | "dFdy"
    | "dFdyCoarse"
    | "dFdyFine"
    | "distance"
    | "dot"
    | "EmitStreamVertex"
    | "EmitVertex"
    | "EndPrimitive"
    | "EndStreamPrimitive"
    | "equal"
    | "exp"
    | "exp2"
    | "faceforward"
    | "findLSB"
    | "findMSB"
    | "floatBitsToInt"
    | "floatBitsToUint"
    | "floor"
    | "fma"
    | "fract"
    | "frexp"
    | "fwidth"
    | "fwidthCoarse"
    | "fwidthFine"
    | "gl_ClipDistance"
    | "gl_CullDistance"
    | "gl_FragCoord"
    | "gl_FragDepth"
    | "gl_FrontFacing"
    | "gl_GlobalInvocationID"
    | "gl_HelperInvocation"
    | "gl_InstanceID"
    | "gl_InvocationID"
    | "gl_Layer"
    | "gl_LocalInvocationID"
    | "gl_LocalInvocationIndex"
    | "gl_NumSamples"
    | "gl_NumWorkGroups"
    | "gl_PatchVerticesIn"
    | "gl_PointCoord"
    | "gl_PointSize"
    | "gl_Position"
    | "gl_PrimitiveID"
    | "gl_PrimitiveIDIn"
    | "gl_SampleID"
    | "gl_SampleMask"
    | "gl_SampleMaskIn"
    | "gl_SamplePosition"
    | "gl_TessCoord"
    | "gl_TessLevelInner"
    | "gl_TessLevelOuter"
    | "gl_VertexID"
    | "gl_ViewportIndex"
    | "gl_WorkGroupID"
    | "gl_WorkGroupSize"
    | "greaterThan"
    | "greaterThanEqual"
    | "groupMemoryBarrier"
    | "imageAtomicAdd"
    | "imageAtomicAnd"
    | "imageAtomicCompSwap"
    | "imageAtomicExchange"
    | "imageAtomicMax"
    | "imageAtomicMin"
    | "imageAtomicOr"
    | "imageAtomicXor"
    | "imageLoad"
    | "imageSamples"
    | "imageSize"
    | "imageStore"
    | "imulExtended"
    | "intBitsToFloat"
    | "interpolateAtCentroid"
    | "interpolateAtOffset"
    | "interpolateAtSample"
    | "inverse"
    | "inversesqrt"
    | "isinf"
    | "isnan"
    | "ldexp"
    | "length"
    | "lessThan"
    | "lessThanEqual"
    | "log"
    | "log2"
    | "matrixCompMult"
    | "max"
    | "memoryBarrier"
    | "memoryBarrierAtomicCounter"
    | "memoryBarrierBuffer"
    | "memoryBarrierImage"
    | "memoryBarrierShared"
    | "min"
    | "mix"
    | "mod"
    | "modf"
    | "noise"
    | "noise1"
    | "noise2"
    | "noise3"
    | "noise4"
    | "normalize"
    | "not"
    | "notEqual"
    | "outerProduct"
    | "packDouble2x32"
    | "packHalf2x16"
    | "packSnorm2x16"
    | "packSnorm4x8"
    | "packUnorm"
    | "packUnorm2x16"
    | "packUnorm4x8"
    | "pow"
    | "radians"
    | "reflect"
    | "refract"
    | "round"
    | "roundEven"
    | "sign"
    | "sin"
    | "sinh"
    | "smoothstep"
    | "sqrt"
    | "step"
    | "tan"
    | "tanh"
    | "texelFetch"
    | "texelFetchOffset"
    | "texture"
    | "textureGather"
    | "textureGatherOffset"
    | "textureGatherOffsets"
    | "textureGrad"
    | "textureGradOffset"
    | "textureLod"
    | "textureLodOffset"
    | "textureOffset"
    | "textureProj"
    | "textureProjGrad"
    | "textureProjGradOffset"
    | "textureProjLod"
    | "textureProjLodOffset"
    | "textureProjOffset"
    | "textureQueryLevels"
    | "textureQueryLod"
    | "textureSamples"
    | "textureSize"
    | "transpose"
    | "trunc"
    | "uaddCarry"
    | "uintBitsToFloat"
    | "umulExtended"
    | "unpackDouble2x32"
    | "unpackHalf2x16"
    | "unpackSnorm2x16"
    | "unpackSnorm4x8"
    | "unpackUnorm"
    | "unpackUnorm2x16"
    | "unpackUnorm4x8"
    | "usubBorrow";
*/