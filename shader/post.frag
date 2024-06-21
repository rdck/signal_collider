#version 450 core

out vec4 FragColor;
in vec2 uv;
uniform sampler2D screen_texture;

#define BLUR .021
#define CA_AMT 1.012

void main()
{ 
  vec4 a = texture(screen_texture, uv);

#ifdef ABBERATION

  vec2 edge = smoothstep(0., BLUR, uv) * (1. - smoothstep(1. - BLUR, 1., uv));

  FragColor.rgb = vec3(
      texture(screen_texture, (uv - .5) * CA_AMT + .5).r,
      texture(screen_texture, uv).g,
      texture(screen_texture, (uv - .5) / CA_AMT +.5).b
      ) * edge.x * edge.y;

  FragColor.a = 1.f;

#else

  FragColor = a;

#endif

}
