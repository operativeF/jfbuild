
inline const char default_polymost_fs_glsl[] = {R"(
#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
precision lowp float;
#  define o_fragcolour gl_FragColor
#elif __VERSION__ < 140
#  define mediump
#  define o_fragcolour gl_FragColor
#else
#  define varying in
#  define texture2D texture
out vec4 o_fragcolour;
#endif

uniform sampler2D u_texture;
uniform sampler2D u_glowtexture;
uniform vec4 u_colour;
uniform float u_alphacut;
uniform vec4 u_fogcolour;
uniform float u_fogdensity;

varying mediump vec2 v_texcoord;

vec4 applyfog(vec4 inputcolour) {
    const float LOG2_E = 1.442695;
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float densdist = u_fogdensity * dist;
    float amount = 1.0 - clamp(exp2(-densdist * densdist * LOG2_E), 0.0, 1.0);
    return mix(inputcolour, u_fogcolour, amount);
}

void main()
{
    vec4 texcolour;
    vec4 glowcolour;

    texcolour = texture2D(u_texture, v_texcoord);
    glowcolour = texture2D(u_glowtexture, v_texcoord);

    if (texcolour.a < u_alphacut) {
        discard;
    }

    texcolour = applyfog(texcolour);
    o_fragcolour = mix(texcolour * u_colour, glowcolour, glowcolour.a);
}
)"};

inline const char default_polymost_vs_glsl[] = {R"(
#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
#elif __VERSION__ < 140
#  define mediump
#else
#  define attribute in
#  define varying out
#endif

attribute vec3 a_vertex;
attribute mediump vec2 a_texcoord;
varying mediump vec2 v_texcoord;

uniform mat4 u_modelview;
uniform mat4 u_projection;

void main()
{
    v_texcoord = a_texcoord;
    gl_Position = u_projection * u_modelview * vec4(a_vertex, 1.0);
}
)"};

inline const char default_polymostaux_fs_glsl[] = {R"(
#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
precision lowp float;
precision lowp int;
#  define o_fragcolour gl_FragColor
#elif __VERSION__ < 140
#  define mediump
#  define o_fragcolour gl_FragColor
#else
#  define varying in
#  define texture2D texture
out vec4 o_fragcolour;
#endif

uniform sampler2D u_texture;
uniform vec4 u_colour;
uniform vec4 u_bgcolour;
uniform int u_mode;

varying mediump vec2 v_texcoord;

void main()
{
    vec4 pixel;

    if (u_mode == 0) {
        // Text.
        pixel = texture2D(u_texture, v_texcoord);
        o_fragcolour = mix(u_bgcolour, u_colour, pixel.a);
    } else if (u_mode == 1) {
        // Tile screen.
        pixel = texture2D(u_texture, v_texcoord);
        o_fragcolour = mix(u_bgcolour, pixel, pixel.a);
    } else if (u_mode == 2) {
        // Foreground colour.
        o_fragcolour = u_colour;
    }
}
)"};

inline const char default_polymostaux_vs_glsl[] = {R"(
#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
#elif __VERSION__ < 140
#  define mediump
#else
#  define attribute in
#  define varying out
#endif

attribute vec3 a_vertex;
attribute mediump vec2 a_texcoord;

uniform mat4 u_projection;

varying mediump vec2 v_texcoord;

void main()
{
    v_texcoord = a_texcoord;
    gl_Position = u_projection * vec4(a_vertex, 1.0);
}
)"};
