#version 450 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv_in;
layout (location = 2) in vec4 color_in;

uniform mat4 projection;
  
out vec2 uv;
out vec4 color;

void main()
{
  uv = uv_in;
  color = color_in;
  gl_Position = projection * vec4(pos.x, pos.y, 0.0f, 1.0f);
}
