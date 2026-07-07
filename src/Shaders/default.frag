#version 120
#extension GL_EXT_texture_array : enable
varying vec3 color;
varying vec2 texCoord;
varying float texLayer;
varying float fog_depth;
uniform sampler2DArray tex0;
uniform vec3 fog_color;
uniform float fog_start;
uniform float fog_end;
void main()
{
    vec4 tex = texture2DArray(tex0, vec3(texCoord, texLayer)) * vec4(color, 1.0);
    if (texLayer > 7.5 && texLayer < 8.5 && tex.a < 0.5) discard;
    float fog_t = clamp((fog_depth - fog_start) / (fog_end - fog_start), 0.0, 1.0);
    float alpha = 1.0;
    if (texLayer > 4.5 && texLayer < 5.5) alpha = 0.58;
    gl_FragColor = vec4(mix(tex.rgb, fog_color, fog_t), alpha);
}
