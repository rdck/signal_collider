#include <stdlib.h>
#include "display.h"
#include "glad/gl.h"
#include "shader/primary.vert.h"
#include "shader/primary.frag.h"
#include "shader/post.vert.h"
#include "shader/post.frag.h"

#define LOG_BUFFER_SIZE 512
#define MAX_SPRITES 0x8000
#define SPRITE_VERTICES 6 // two triangles
#define FB_FILTER GL_LINEAR
#define TEXTURE_FILTER GL_LINEAR
#define VERTEX_STRIDE 5

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

typedef struct {
  F32 x; F32 y; // position
  F32 u; F32 v; // texture coordinates
  U32 color;
} Vertex;

static Sprite display_sprite_buffer[MAX_SPRITES] = {0};
static Vertex display_vertex_buffer[SPRITE_VERTICES * MAX_SPRITES] = {0};
static S32 display_sprite_index = 0;

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

TextureID display_load_image(const Byte* image, V2S dimensions)
{
  if (image) {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);  
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TEXTURE_FILTER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TEXTURE_FILTER);
#ifdef DISPLAY_SUPPORT_MONOCHROME
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#endif
    glTexImage2D(
        GL_TEXTURE_2D,
        0,                  // level
        GL_RGBA,            // internal format
        dimensions.x,       // width
        dimensions.y,       // height
        0,                  // border (must be 0)
        GL_RGBA,            // format
        GL_UNSIGNED_BYTE,   // type of pixel data
        image
        );
    return id;
  } else {
    return 0;
  }
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

  // position attribute
  glVertexAttribPointer(
      0,                            // index
      2,                            // size
      GL_FLOAT,                     // type
      GL_FALSE,                     // normalized
      VERTEX_STRIDE * sizeof(F32),  // stride
      (Void*)0                      // offset
      );
  glEnableVertexAttribArray(0);

  // texture coordinate attribute
  glVertexAttribPointer(
      1,                            // index
      2,                            // size
      GL_FLOAT,                     // type
      GL_FALSE,                     // normalized
      VERTEX_STRIDE * sizeof(F32),  // stride
      (Void*)(2 * sizeof(F32))      // offset
      );
  glEnableVertexAttribArray(1);

  // color attribute
  glVertexAttribPointer(
      2,                            // index
      4,                            // size
      GL_UNSIGNED_BYTE,             // type
      GL_TRUE,                      // normalized
      VERTEX_STRIDE * sizeof(F32),  // stride
      (Void*)(4 * sizeof(F32))      // offset
      );
  glEnableVertexAttribArray(2);

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

Void display_begin_frame()
{
  glBindFramebuffer(GL_FRAMEBUFFER, ctx.fbo);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(ctx.program);
  glViewport(0, 0, ctx.render_resolution.x, ctx.render_resolution.y);
  glBindVertexArray(ctx.VAO);
  glEnable(GL_BLEND);
}

Void display_end_frame()
{
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

Void display_bind_texture(TextureID id)
{
  glBindTexture(GL_TEXTURE_2D, id);
}

Void display_begin_draw()
{
  display_sprite_index = 0;
}

Void display_end_draw()
{
  Vertex* const vertices = display_vertex_buffer;

  for (S32 i = 0; i < display_sprite_index; i++) {

    const S32 vi = SPRITE_VERTICES * i;
    const Sprite* const sprite = &display_sprite_buffer[i];

    // top left
    vertices[vi + 0].x = sprite->root.x;
    vertices[vi + 0].y = sprite->root.y;
    vertices[vi + 0].u = sprite->ta.u;
    vertices[vi + 0].v = sprite->ta.v;
    vertices[vi + 0].color = sprite->color;

    // bottom left
    vertices[vi + 1].x = sprite->root.x;
    vertices[vi + 1].y = sprite->root.y + sprite->size.y;
    vertices[vi + 1].u = sprite->ta.u;
    vertices[vi + 1].v = sprite->tb.v;
    vertices[vi + 1].color = sprite->color;

    // bottom right
    vertices[vi + 2].x = sprite->root.x + sprite->size.x;
    vertices[vi + 2].y = sprite->root.y + sprite->size.y;
    vertices[vi + 2].u = sprite->tb.u;
    vertices[vi + 2].v = sprite->tb.v;
    vertices[vi + 2].color = sprite->color;

    // bottom right
    vertices[vi + 3].x = sprite->root.x + sprite->size.x;
    vertices[vi + 3].y = sprite->root.y + sprite->size.y;
    vertices[vi + 3].u = sprite->tb.u;
    vertices[vi + 3].v = sprite->tb.v;
    vertices[vi + 3].color = sprite->color;

    // top right
    vertices[vi + 4].x = sprite->root.x + sprite->size.x;
    vertices[vi + 4].y = sprite->root.y;
    vertices[vi + 4].u = sprite->tb.u;
    vertices[vi + 4].v = sprite->ta.v;
    vertices[vi + 4].color = sprite->color;

    // top left
    vertices[vi + 5].x = sprite->root.x;
    vertices[vi + 5].y = sprite->root.y;
    vertices[vi + 5].u = sprite->ta.u;
    vertices[vi + 5].v = sprite->ta.v;
    vertices[vi + 5].color = sprite->color;

  }

  glBindBuffer(GL_ARRAY_BUFFER, ctx.VBO);
  glBufferData(
      GL_ARRAY_BUFFER,    // target
      SPRITE_VERTICES * display_sprite_index * sizeof(Vertex), // size in bytes
      vertices,           // data
      GL_DYNAMIC_DRAW     // usage
      );

  glBindVertexArray(ctx.VAO);
  glDrawArrays(GL_TRIANGLES, 0, SPRITE_VERTICES * display_sprite_index);
}
