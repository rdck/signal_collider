#version 450 core

in vec2 uv;
in vec4 color;
in float depth;

uniform sampler2D tex;
uniform float blend;

layout (location = 0) out vec4 out_color;
  
void main()
{

    // vec4 color_sample = texture(tex, uv);
    // color_sample.rgb *= color_sample.a;

    // vec4 color_pre = color;
    // color_pre.rgb *= color_pre.a;

    // // color.rgb *= color.a;

    // vec4 a = vec4(1.0f, 1.0f, 1.0f, color_sample.r);
    // vec4 mixed = mix(a * color_pre, color_sample * color_pre, blend);

    // // mixed.rgb *= mixed.a;
    // // if (mixed.a < 0.05) {
    // //     discard;
    // // }

    // out_color = mixed;
    // gl_FragDepth = depth;

    vec4 color_sample = texture(tex, uv);
    color_sample *= color;
    color_sample.rgb *= color_sample.a;

    // vec4 pre = color;
    // pre.rgb *= pre.a;

    // color_sample *= pre;
    out_color = color_sample;

}
