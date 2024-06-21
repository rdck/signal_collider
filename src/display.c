#include <stdlib.h>
#include "display.h"
#include "glad/gl.h"
#include "shader/primary.vert.h"
#include "shader/primary.frag.h"
#include "shader/post.vert.h"
#include "shader/post.frag.h"

#define LOG_BUFFER_SIZE 512
#define MAX_SPRITES 0x4000
#define FB_FILTER GL_LINEAR
#define TEXTURE_FILTER GL_LINEAR

typedef struct {

  V2S render_resolution;
  V2S window_resolution;

  GLuint program;
  GLuint VBO;
  GLuint VAO;

  GLuint screen_program;
  GLuint fbo;
  GLuint fb_color;
  GLuint screen_vbo;
  GLuint screen_vao;

  GLint projection;
  GLint blend;

} DisplayContext;

static Sprite display_sprite_buffer[MAX_SPRITES] = {0};
static S32 display_sprite_index = 0;

typedef struct {
  F32 x; F32 y; // position
  F32 u; F32 v; // texture coordinates
  U32 color;
  F32 depth;
} Vertex;

static DisplayContext ctx = {0};

static F32 quad_vertices[] = {  
  -1.0f, +1.0f, +0.0f, +1.0f,
  -1.0f, -1.0f, +0.0f, +0.0f,
  +1.0f, -1.0f, +1.0f, +0.0f,

  -1.0f, +1.0f, +0.0f, +1.0f,
  +1.0f, -1.0f, +1.0f, +0.0f,
  +1.0f, +1.0f, +1.0f, +1.0f,
};

Void display_sprite(Sprite sprite)
{
  if (display_sprite_index < MAX_SPRITES) {
    display_sprite_buffer[display_sprite_index] = sprite;
    display_sprite_index += 1;
  }
}

static Void GLAPIENTRY gl_message_callback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const Void* user_param
    )
{
  UNUSED_PARAMETER(source);
  UNUSED_PARAMETER(type);
  UNUSED_PARAMETER(id);
  UNUSED_PARAMETER(severity);
  UNUSED_PARAMETER(length);
  UNUSED_PARAMETER(message);
  UNUSED_PARAMETER(user_param);
  ASSERT(severity == GL_DEBUG_SEVERITY_NOTIFICATION);
}

// @rdk: don't set alignment when not necessary
// @rdk: inline
static TextureID load_png(
    S32 x,
    S32 y,
    const Byte* data,
    GLint internal_format,
    GLint format,
    GLint wrap,
    GLint filter
    )
{
  ASSERT(data);
  GLuint id;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // necessary for monochrome textures
  glTexImage2D(
      GL_TEXTURE_2D,
      0,                          // level
      internal_format,            // internal format
      x,                          // width
      y,                          // height
      0,                          // border (must be 0)
      format,                     // format
      GL_UNSIGNED_BYTE,           // type of pixel data
      data
      );
  // @rdk: Do we need to generate the mipmap?
  glGenerateMipmap(GL_TEXTURE_2D);
  return id;
}

TextureID display_load_image(const Byte* image, V2S dimensions)
{
  ASSERT(image);
  const TextureID id = load_png(
      dimensions.x, dimensions.y,
      image,
      GL_RGBA,
      GL_RGBA,
      GL_CLAMP_TO_EDGE,
      TEXTURE_FILTER
      );
  return id;
}

// returns 0 on failure
static GLuint compile_shader(GLenum type, GLchar* source, GLint length)
{
  GLuint shader_id = glCreateShader(type);
  if (shader_id) {
    GLchar* sources[1] = {source};
    glShaderSource(shader_id, 1, sources, &length);
    glCompileShader(shader_id);
    GLint success = 0;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);
    if (!success) {
      Char log_buffer[LOG_BUFFER_SIZE];
      glGetShaderInfoLog(shader_id, LOG_BUFFER_SIZE, NULL, log_buffer);
      shader_id = 0;
    }
  }
  return shader_id;
}

// returns 0 on failure
static GLuint link_program(GLuint vertex, GLuint fragment)
{
  GLuint program = glCreateProgram();
  if (program) {
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
      Char log_buffer[LOG_BUFFER_SIZE];
      glGetProgramInfoLog(program, LOG_BUFFER_SIZE, NULL, log_buffer);
      program = 0;
    }
  }
  return program;
}

// returns 0 on failure
static GLuint compile_program()
{
  GLuint program = 0;
  GLuint vertex = compile_shader(GL_VERTEX_SHADER, (GLchar*) shader_primary_vert, shader_primary_vert_len);
  GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, (GLchar*) shader_primary_frag, shader_primary_frag_len);
  if (vertex && fragment) {
    program = link_program(vertex, fragment);
  }
  if (vertex) { glDeleteShader(vertex); }
  if (fragment) { glDeleteShader(fragment); }
  return program;
}

// @rdk: unify with compile_program
static GLuint compile_post()
{
  GLuint program = 0;
  GLuint vertex = compile_shader(GL_VERTEX_SHADER, (GLchar*) shader_post_vert, shader_post_vert_len);
  GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, (GLchar*) shader_post_frag, shader_post_frag_len);
  if (vertex && fragment) {
    program = link_program(vertex, fragment);
  }
  if (vertex) { glDeleteShader(vertex); }
  if (fragment) { glDeleteShader(fragment); }
  return program;
}

Void display_init(V2S window, V2S render)
{
  // @rdk: disable GL debug output in release builds

  ctx.window_resolution = window;
  ctx.render_resolution = render;

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(gl_message_callback, 0);

  ctx.program = compile_program();
  ctx.screen_program = compile_post();

  ////////////////////////////////////////////////////////////////////////////////

  glUseProgram(ctx.screen_program);

  glGenVertexArrays(1, &ctx.screen_vao);
  glGenBuffers(1, &ctx.screen_vbo);
  glBindVertexArray(ctx.screen_vao);
  glBindBuffer(GL_ARRAY_BUFFER, ctx.screen_vbo);

  glVertexAttribPointer(
      0,                          // index
      2,                          // size
      GL_FLOAT,                   // type
      GL_FALSE,                   // normalized
      4 * sizeof(F32),            // stride
      (Void*)0                    // offset
      );
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      1,                          // index
      2,                          // size
      GL_FLOAT,                   // type
      GL_FALSE,                   // normalized
      4 * sizeof(F32),            // stride
      (Void*)(2 * sizeof(F32))    // offset
      );
  glEnableVertexAttribArray(1);

  glBufferData(
      GL_ARRAY_BUFFER,        // target
      sizeof(quad_vertices),  // size in bytes
      quad_vertices,          // data
      GL_STATIC_DRAW          // usage
      );

  ////////////////////////////////////////////////////////////////////////////////

  glUseProgram(ctx.program);

  glGenVertexArrays(1, &ctx.VAO);
  glGenBuffers(1, &ctx.VBO);
  glBindVertexArray(ctx.VAO);
  glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);

  glVertexAttribPointer(
      0,                          // index
      2,                          // size
      GL_FLOAT,                   // type
      GL_FALSE,                   // normalized
      6 * sizeof(F32),            // stride
      (Void*)0                    // offset
      );
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(
      1,                          // index
      2,                          // size
      GL_FLOAT,                   // type
      GL_FALSE,                   // normalized
      6 * sizeof(F32),            // stride
      (Void*)(2 * sizeof(F32))    // offset
      );
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(
      2,                          // index
      4,                          // size
      GL_UNSIGNED_BYTE,           // type
      GL_TRUE,                    // normalized
      6 * sizeof(F32),            // stride
      (Void*)(4 * sizeof(F32))    // offset
      );
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(
      3,                          // index
      1,                          // size
      GL_FLOAT,                   // type
      GL_FALSE,                   // normalized
      6 * sizeof(F32),            // stride
      (Void*)(5 * sizeof(F32))    // offset
      );
  glEnableVertexAttribArray(3);

  ctx.projection = glGetUniformLocation(ctx.program, "projection");
  M4F ortho = HMM_Orthographic_RH_NO(
      0.f,                        // left
      (F32) render.x,             // right
      (F32) render.y,             // bottom
      0.f,                        // top
      -1.f,                       // near
      1.f                         // far
      );
  glUniformMatrix4fv(ctx.projection, 1, GL_FALSE, (GLfloat*) &ortho.Elements);

  ctx.blend = glGetUniformLocation(ctx.program, "blend");
  glUniform1f(ctx.blend, 0.0f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // framebuffer setup

  glGenFramebuffers(1, &ctx.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo);

  glGenTextures(1, &ctx.fb_color);
  glBindTexture(GL_TEXTURE_2D, ctx.fb_color);

  glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_RGBA,
      render.x, render.y,
      0,
      GL_RGBA,
      GL_UNSIGNED_BYTE,
      NULL
      );

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, FB_FILTER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, FB_FILTER);  

  glFramebufferTexture2D(
      GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      ctx.fb_color,
      0
      );
}

static Void draw_rect(Sprite sprite)
{
  const Vertex top_left = {
    .x = sprite.root.x,
    .y = sprite.root.y,
    .u = sprite.ta.u,
    .v = sprite.ta.v,
    .color = sprite.color,
    .depth = sprite.depth,
  };
  const Vertex top_right = {
    .x = sprite.root.x + sprite.size.x,
    .y = sprite.root.y,
    .u = sprite.tb.u,
    .v = sprite.ta.v,
    .color = sprite.color,
    .depth = sprite.depth,
  };
  const Vertex bot_left = {
    .x = sprite.root.x,
    .y = sprite.root.y + sprite.size.y,
    .u = sprite.ta.u,
    .v = sprite.tb.v,
    .color = sprite.color,
    .depth = sprite.depth,
  };
  const Vertex bot_right = {
    .x = sprite.root.x + sprite.size.x,
    .y = sprite.root.y + sprite.size.y,
    .u = sprite.tb.u,
    .v = sprite.tb.v,
    .color = sprite.color,
    .depth = sprite.depth,
  };

  Vertex vertices[6];
  vertices[0] = top_left;
  vertices[1] = bot_left;
  vertices[2] = bot_right;
  vertices[3] = bot_right;
  vertices[4] = top_right;
  vertices[5] = top_left;

  // @rdk: remove this when possible
  const F32 blend = 1.f;
  glUniform1f(ctx.blend, blend);

  glBindTexture(GL_TEXTURE_2D, sprite.texture);

  glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
  glBufferData(
      GL_ARRAY_BUFFER,    // target
      sizeof(vertices),   // size in bytes
      vertices,           // data
      GL_DYNAMIC_DRAW     // usage
      );

  glBindVertexArray(ctx.VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

Void display_begin_frame()
{
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo);
  glClear(GL_COLOR_BUFFER_BIT);
}

static S32 compare_depth(const Sprite* a, const Sprite* b)
{
  return a->depth == b->depth ? 0 : (a->depth < b->depth ? 1 : -1); 
}

Void display_end_frame()
{
  glUseProgram(ctx.program);
  glViewport(0, 0, ctx.render_resolution.x, ctx.render_resolution.y);
  glBindVertexArray(ctx.VAO);
  glEnable(GL_BLEND);

  const Size n = display_sprite_index;
  qsort(display_sprite_buffer, n, sizeof(Sprite), compare_depth);
  for (S32 i = 0; i < n; i++) {
    draw_rect(display_sprite_buffer[i]);
  }

  memset(display_sprite_buffer, 0, sizeof(display_sprite_buffer));
  display_sprite_index = 0;

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(ctx.screen_program);
  glViewport(0, 0, ctx.window_resolution.x, ctx.window_resolution.y);
  glDisable(GL_BLEND);
  glBindVertexArray(ctx.screen_vao);
  glBindTexture(GL_TEXTURE_2D, ctx.fb_color);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

U32 display_color_lerp(U32 a, U32 b, F32 t)
{
  Color32* const ap = (Color32*) &a;
  Color32* const bp = (Color32*) &b;

  Color32 ac = *ap;
  Color32 bc = *bp;

  Color32 out;
  out.r = (U8) f32_lerp((F32) ac.r, (F32) bc.r, t);
  out.g = (U8) f32_lerp((F32) ac.g, (F32) bc.g, t);
  out.b = (U8) f32_lerp((F32) ac.b, (F32) bc.b, t);
  out.a = (U8) f32_lerp((F32) ac.a, (F32) bc.a, t);

  U32* const outp = (U32*) &out;
  return *outp;
}
