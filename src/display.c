#include <stdio.h> // @K-MONK: replace, when possible
#include <stdlib.h>

#include "display.h"
#include "glad/gl.h"
#include "shader/primary.vert.h"
#include "shader/primary.frag.h"
#include "shader/post.vert.h"
#include "shader/post.frag.h"

#define Display_LOG_BUFFER_SIZE 512
#define Display_MAX_SPRITES 0x4000
#define Display_FB_FILTER GL_LINEAR
#define Display_TEXTURE_FILTER GL_LINEAR

#define PACK_MIN ' '
#define PACK_MAX '~'
#define PACK_COUNT (PACK_MAX + 1 - PACK_MIN)
#define FONT_OVERSAMPLING 2
#define ATLAS_WIDTH (FONT_OVERSAMPLING * 16) // estimated width of font atlas, in glyphs

#define DOWNX 24
#define DOWNY 24

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

} Display_Context;

static Display_Sprite Display_sprite_buffer[Display_MAX_SPRITES] = {0};
static S32 Display_sprite_index = 0;

typedef struct {
    F32 x; F32 y; // position
    F32 u; F32 v; // texture coordinates
    U32 color;
    F32 depth;
} Display_Vertex;

static Display_Context Display_ctx = {0};

static F32 quad_vertices[] = {  

    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f,

};

static Void Display_push_sprite(Display_Sprite sprite)
{
  if (Display_sprite_index < Display_MAX_SPRITES) {
    Display_sprite_buffer[Display_sprite_index] = sprite;
    Display_sprite_index += 1;
  }
}

Void GLAPIENTRY gl_message_callback(
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

// @K-MONK: don't set alignment when not necessary
// @K-MONK: inline
static Display_TextureID _load_png(
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
    // @K-MONK: Do we need to generate the mipmap?
    glGenerateMipmap(GL_TEXTURE_2D);
    return id;
}

Display_TextureID Display_load_image(const Byte* image, V2S dimensions)
{
    ASSERT(image);
    const Display_TextureID id = _load_png(
            dimensions.x, dimensions.y,
            image,
            GL_RGBA,
            GL_RGBA,
            GL_CLAMP_TO_EDGE,
            Display_TEXTURE_FILTER
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
            Char log_buffer[Display_LOG_BUFFER_SIZE];
            glGetShaderInfoLog(shader_id, Display_LOG_BUFFER_SIZE, NULL, log_buffer);
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
            Char log_buffer[Display_LOG_BUFFER_SIZE];
            glGetProgramInfoLog(program, Display_LOG_BUFFER_SIZE, NULL, log_buffer);
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

// @K-MONK: unify with compile_program
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

Void Display_init(V2S window, V2S render)
{

  // @rdk: disable GL debug output in release builds

  Display_ctx.window_resolution = window;
  Display_ctx.render_resolution = render;

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(gl_message_callback, 0);

  Display_ctx.program = compile_program();
  Display_ctx.screen_program = compile_post();

  ////////////////////////////////////////////////////////////////////////////////

  glUseProgram(Display_ctx.screen_program);

  glGenVertexArrays(1, &Display_ctx.screen_vao);
  glGenBuffers(1, &Display_ctx.screen_vbo);
  glBindVertexArray(Display_ctx.screen_vao);
  glBindBuffer(GL_ARRAY_BUFFER, Display_ctx.screen_vbo);

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

  glUseProgram(Display_ctx.program);

  glGenVertexArrays(1, &Display_ctx.VAO);
  glGenBuffers(1, &Display_ctx.VBO);
  glBindVertexArray(Display_ctx.VAO);
  glBindBuffer(GL_ARRAY_BUFFER, Display_ctx.VBO);

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

#if 0
  glViewport(0, 0, window_width, window_height);
#endif
  Display_ctx.projection = glGetUniformLocation(Display_ctx.program, "projection");
  M4F ortho = HMM_Orthographic_RH_NO(
      0.f,                        // left
      (F32) render.x,             // right
      (F32) render.y,             // bottom
      0.f,                        // top
      -1.f,                       // near
      1.f                         // far
      );
  glUniformMatrix4fv(Display_ctx.projection, 1, GL_FALSE, (GLfloat*) &ortho.Elements);

  Display_ctx.blend = glGetUniformLocation(Display_ctx.program, "blend");
  glUniform1f(Display_ctx.blend, 0.0f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // framebuffer setup

  glGenFramebuffers(1, &Display_ctx.fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, Display_ctx.fbo);

  glGenTextures(1, &Display_ctx.fb_color);
  glBindTexture(GL_TEXTURE_2D, Display_ctx.fb_color);

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

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, Display_FB_FILTER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, Display_FB_FILTER);  

  glFramebufferTexture2D(
      GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      Display_ctx.fb_color,
      0
      );

}

#if 0
        _depth_rect(
                c->texture,
                c->color,
                c->depth,
                c->x, c->y, c->au, c->av,
                c->x + c->w, c->y + c->h, c->bu, c->bv
                );

static Void _depth_rect(
        Display_TextureID tid,
        U32 color,
        F32 depth,
        F32 ax, F32 ay, F32 au, F32 av,
        F32 bx, F32 by, F32 bu, F32 bv
        )
{
}
#endif

static Void _depth_rect(Display_Sprite sprite)
{
#if 0
    const Display_Vertex top_left = {
        .x = ax,
        .y = ay,
        .u = au,
        .v = av,
        .color = color,
        .depth = depth,
    };
    const Display_Vertex top_right = {
        .x = bx,
        .y = ay,
        .u = bu,
        .v = av,
        .color = color,
        .depth = depth,
    };
    const Display_Vertex bot_left = {
        .x = ax,
        .y = by,
        .u = au,
        .v = bv,
        .color = color,
        .depth = depth,
    };
    const Display_Vertex bot_right = {
        .x = bx,
        .y = by,
        .u = bu,
        .v = bv,
        .color = color,
        .depth = depth,
    };
#endif
    const Display_Vertex top_left = {
        .x = sprite.root.x,
        .y = sprite.root.y,
        .u = sprite.ta.u,
        .v = sprite.ta.v,
        .color = sprite.color,
        .depth = sprite.depth,
    };
    const Display_Vertex top_right = {
        .x = sprite.root.x + sprite.size.x,
        .y = sprite.root.y,
        .u = sprite.tb.u,
        .v = sprite.ta.v,
        .color = sprite.color,
        .depth = sprite.depth,
    };
    const Display_Vertex bot_left = {
        .x = sprite.root.x,
        .y = sprite.root.y + sprite.size.y,
        .u = sprite.ta.u,
        .v = sprite.tb.v,
        .color = sprite.color,
        .depth = sprite.depth,
    };
    const Display_Vertex bot_right = {
        .x = sprite.root.x + sprite.size.x,
        .y = sprite.root.y + sprite.size.y,
        .u = sprite.tb.u,
        .v = sprite.tb.v,
        .color = sprite.color,
        .depth = sprite.depth,
    };

    Display_Vertex vertices[6];
    vertices[0] = top_left;
    vertices[1] = bot_left;
    vertices[2] = bot_right;
    vertices[3] = bot_right;
    vertices[4] = top_right;
    vertices[5] = top_left;

    // @rdk: remove this when possible
    const F32 blend = 1.f;
    glUniform1f(Display_ctx.blend, blend);

    glBindTexture(GL_TEXTURE_2D, sprite.texture);

    glBindBuffer(GL_ARRAY_BUFFER, Display_ctx.VBO);
    glBufferData(
            GL_ARRAY_BUFFER,    // target
            sizeof(vertices),   // size in bytes
            vertices,           // data
            GL_DYNAMIC_DRAW     // usage
            );

    glBindVertexArray(Display_ctx.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

Void Display_begin_frame()
{

    glBindFramebuffer(GL_FRAMEBUFFER, Display_ctx.fbo);
    // glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // // glClearColor(0.04f, 0.04f, 0.04f, 1.0f);
    // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    // // glClearColor(0.4f, 0.4f, 0.4f, 1.f);
    // // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glClear(GL_COLOR_BUFFER_BIT);

}

static S32 _compare_depth(const Display_Sprite* a, const Display_Sprite* b)
{
    return a->depth == b->depth ? 0 : (a->depth < b->depth ? 1 : -1); 
}

Void Display_end_frame()
{

    glUseProgram(Display_ctx.program);
    glViewport(0, 0, Display_ctx.render_resolution.x, Display_ctx.render_resolution.y);
    glBindVertexArray(Display_ctx.VAO);
    glEnable(GL_BLEND);

    const Size n = Display_sprite_index;
    qsort(Display_sprite_buffer, n, sizeof(Display_Sprite), _compare_depth);
    for (S32 i = 0; i < n; i++) {
#if 0
        _depth_rect(
                c->texture,
                c->color,
                c->depth,
                c->x, c->y, c->au, c->av,
                c->x + c->w, c->y + c->h, c->bu, c->bv
                );
#endif
        _depth_rect(Display_sprite_buffer[i]);
    }

    memset(Display_sprite_buffer, 0, sizeof(Display_sprite_buffer));
    Display_sprite_index = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(Display_ctx.screen_program);
    glViewport(0, 0, Display_ctx.window_resolution.x, Display_ctx.window_resolution.y);
    glDisable(GL_BLEND);
    glBindVertexArray(Display_ctx.screen_vao);
    glBindTexture(GL_TEXTURE_2D, Display_ctx.fb_color);
    glDrawArrays(GL_TRIANGLES, 0, 6);

}

U32 Display_color_lerp(U32 a, U32 b, F32 t)
{

  Display_Color* const ap = (Display_Color*) &a;
  Display_Color* const bp = (Display_Color*) &b;

  Display_Color ac = *ap;
  Display_Color bc = *bp;

  Display_Color out;
  out.r = (U8) f32_lerp((F32) ac.r, (F32) bc.r, t);
  out.g = (U8) f32_lerp((F32) ac.g, (F32) bc.g, t);
  out.b = (U8) f32_lerp((F32) ac.b, (F32) bc.b, t);
  out.a = (U8) f32_lerp((F32) ac.a, (F32) bc.a, t);

  U32* const outp = (U32*) &out;
  return *outp;

}

#if 0
Void Display_sprite(
        Display_TextureID texture_id,
        F32 au, F32 av,
        F32 bu, F32 bv,
        U32 color,
        F32 depth,
        F32 x, F32 y,
        F32 w, F32 h
        )
{
  Display_Sprite sprite;
  sprite.texture = texture_id;
  sprite.au = au;
  sprite.av = av;
  sprite.bu = bu;
  sprite.bv = bv;
  sprite.color = color;
  sprite.depth = depth;
  sprite.x = x;
  sprite.y = y;
  sprite.w = w;
  sprite.h = h;
  Display_push_sprite(sprite);
}
#endif

Void Display_sprite(Display_Sprite s)
{
  Display_push_sprite(s);
}

#if 0
Void display_load_font(const Byte* ttf, S32 size)
{

    stbtt_fontinfo font = {0};
    const S32 init_result = stbtt_InitFont(&font, ttf, 0);
    ASSERT(init_result != 0);

    const F32 scale = stbtt_ScaleForPixelHeight(&font, (F32) size);

#if 0
    const F32 scale = stbtt_ScaleForMappingEmToPixels(&font, (F32) size);
#endif

    S32 ascent = 0;
    S32 descent = 0;
    S32 line_gap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

#if 0
    const F32 vertical = scale * (ascent - descent + line_gap);
#endif

    const S32 char_area = size * size;
    Byte* const atlas = malloc(Display_ASCII_WIDTH * Display_ASCII_HEIGHT * char_area);
    memset(atlas, 0, Display_ASCII_WIDTH * Display_ASCII_HEIGHT * char_area);

    S32 w = 0;
    S32 h = 0;
    S32 xoff = 0;
    S32 yoff = 0;
    for (Char c = '!'; c <= '~'; c++) {

        Byte* const bitmap = stbtt_GetCodepointBitmap(
                &font,
                scale, scale,
                c,
                &w, &h,
                &xoff, &yoff
                );
        ASSERT(w <= size);
        ASSERT(h <= size);

        const V2S p = display__ascii_coordinate(c);
        for (S32 y = 0; y < h; y++) {
            for (S32 x = 0; x < w; x++) {
                const S32 x0 = p.x * size + xoff + x;
                const S32 y0 = p.y * size + yoff + y;
                const Bool xr = x0 < Display_ASCII_WIDTH  * size;
                const Bool yr = y0 < Display_ASCII_HEIGHT * size;
                if (xr && yr) {
                    atlas[y0 * (Display_ASCII_WIDTH * size) + x0] = bitmap[y * w + x];
                }
            }
        }

        stbtt_FreeBitmap(bitmap, NULL);

    }

    stbi_write_png(
            "font_atlas.png",
            Display_ASCII_WIDTH * size,
            Display_ASCII_HEIGHT * size,
            1,
            atlas,
            Display_ASCII_WIDTH * size
            );

}
#endif
