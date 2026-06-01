#version 330 core
out vec4 FragColor;

in vec2 texCoord;
in vec3 color;
in float texLayer;

uniform sampler2DArray tex0;

void main()
{
   FragColor = texture(tex0, vec3(texCoord, texLayer)) * vec4(color, 1.0);
}