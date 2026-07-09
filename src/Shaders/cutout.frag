#version 120
#extension GL_EXT_texture_array : enable
varying vec3 color;
varying vec2 texCoord;
varying float texLayer;
varying float fog_depth;
varying vec3 world_pos;
uniform sampler2DArray tex0;
uniform vec3 fog_color;
uniform float fog_start;
uniform float fog_end;
uniform vec3 light_dir;
uniform float light_ambient;
uniform float light_diffuse;
void main()
{
    vec4 tex = texture2DArray(tex0, vec3(texCoord, texLayer)) * vec4(color, 1.0);
    if (tex.a < 0.5) discard;
    vec3 nrm = normalize(cross(dFdx(world_pos), dFdy(world_pos)));
    float shade = clamp(light_ambient + light_diffuse * max(dot(nrm, light_dir), 0.0), 0.0, 1.0);
    float fog_t = clamp((fog_depth - fog_start) / (fog_end - fog_start), 0.0, 1.0);
    gl_FragColor = vec4(mix(tex.rgb * shade, fog_color, fog_t), 1.0);
}
