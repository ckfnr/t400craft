#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in float aTexLayer;

out vec3 color;
out vec2 texCoord;
out float texLayer;
out float fog_depth;

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
