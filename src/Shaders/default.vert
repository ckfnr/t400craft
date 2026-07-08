#version 120
#extension GL_EXT_texture_array : enable
attribute vec3 aPos;
attribute vec3 aColor;
attribute vec2 aTexCoord;
attribute float aTexLayer;
varying vec3 color;
varying vec2 texCoord;
varying float texLayer;
varying float fog_depth;
varying vec3 world_pos;
uniform mat4 camMatrix;
uniform mat4 model;
void main()
{
    vec4 wp = model * vec4(aPos, 1.0);
    world_pos = wp.xyz;
    gl_Position = camMatrix * wp;
    fog_depth   = gl_Position.w;
    color       = aColor;
    texCoord    = aTexCoord;
    texLayer    = aTexLayer;
}
