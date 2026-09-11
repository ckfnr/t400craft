//specialized shader for leaves & glass
//hidden faces get culled

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
    bool pane_glass = (texLayer > 8.5 && texLayer < 9.5) ||
                      (texLayer >= 20.0 && texLayer <= 23.5);
    bool stained_glass = texLayer >= 20.0 && texLayer <= 23.5;
    vec3 glass_color = vec3(1.0);
    if (texLayer > 19.5 && texLayer < 20.5) glass_color = vec3(0.60, 0.08, 0.72);
    if (texLayer > 20.5 && texLayer < 21.5) glass_color = vec3(0.08, 0.25, 0.90);
    if (texLayer > 21.5 && texLayer < 22.5) glass_color = vec3(0.10, 0.72, 0.20);
    if (texLayer > 22.5 && texLayer < 23.5) glass_color = vec3(0.90, 0.08, 0.10);
    if (!pane_glass && tex.a < 0.5) discard;
    vec3 nrm = normalize(cross(dFdx(world_pos), dFdy(world_pos)));
    float shade = max(light_ambient + light_diffuse * max(dot(nrm, light_dir), 0.0), 0.28);
    shade = clamp(shade, 0.0, 1.0);
    float fog_t = clamp((fog_depth - fog_start) / (fog_end - fog_start), 0.0, 1.0);
    vec3 base_color = tex.a < 0.5 ? glass_color : tex.rgb;
    float alpha = pane_glass ? (tex.a < 0.5 ? (stained_glass ? 0.39 : 0.18) : (stained_glass ? 0.66 : 0.18)) : 1.0;
    gl_FragColor = vec4(mix(base_color * shade, fog_color, fog_t), alpha);
}
