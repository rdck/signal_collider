#version 450 core

in vec2 uv;
in vec4 color;

uniform sampler2D tex;
uniform float blend;

layout (location = 0) out vec4 out_color;
  
void main()
{
  vec4 color_sample = texture(tex, uv);
  color_sample *= color;
  color_sample.rgb *= color_sample.a;
  out_color = color_sample;
}
