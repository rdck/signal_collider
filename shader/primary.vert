#version 450 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 uv_in;
layout (location = 2) in vec4 color_in;
layout (location = 3) in float depth_in;

uniform mat4 projection;
  
out vec2 uv;
out vec4 color;
out float depth;

void main()
{
  uv = uv_in;
  color = color_in;
  depth = depth_in;
  gl_Position = projection * vec4(pos.x, pos.y, 0.0f, 1.0f);
}
