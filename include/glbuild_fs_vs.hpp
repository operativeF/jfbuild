

inline const char default_glbuild_fs_glsl[] = {R"(
#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
#  define o_fragcolour gl_FragColor
#elif __VERSION__ < 140
#  define lowp
#  define mediump
#  define o_fragcolour gl_FragColor
#else
#  define varying in
#  define texture2D texture
out vec4 o_fragcolour;
#endif

varying mediump vec2 v_texcoord;

uniform sampler2D u_palette;
uniform sampler2D u_frame;

void main()
{
  lowp float pixelvalue;
  lowp vec3 palettevalue;
  pixelvalue = texture2D(u_frame, v_texcoord).r;
  palettevalue = texture2D(u_palette, vec2(pixelvalue, 0.5)).rgb;
  o_fragcolour = vec4(palettevalue, 1.0);
}
)"};



inline const char default_glbuild_vs_glsl[] = {R"(
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

attribute mediump vec2 a_vertex;
attribute mediump vec2 a_texcoord;
varying mediump vec2 v_texcoord;

void main()
{
  v_texcoord = a_texcoord;
  gl_Position = vec4(a_vertex, 0.0, 1.0);
}
)"};
