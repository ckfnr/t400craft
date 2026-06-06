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
uniform mat4 camMatrix;
uniform mat4 model;
void main()
{
    gl_Position = camMatrix * model * vec4(aPos, 1.0);
    fog_depth   = gl_Position.w;
    color       = aColor;
    texCoord    = aTexCoord;
    texLayer    = aTexLayer;
}
