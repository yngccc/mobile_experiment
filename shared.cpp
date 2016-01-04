/* The MIT License (MIT)

   Copyright (c) <2015-2016> <Yang Chen> <yngccc@gmail.com>

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE. */

#ifdef __ANDROID__
#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android_native_app_glue.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "my_activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "my_activity", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "my_activity", __VA_ARGS__))
#define LOGF(...) ((void)__android_log_print(ANDROID_LOG_FATAL, "my_activity", __VA_ARGS__))
#endif

#ifdef __APPLE__
#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

#define LOGI(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGW(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGE(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGF(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#endif

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned char byte;

#define ARRAY_LEN(x) ((int64)(sizeof(x) / sizeof(x[0])))
#define ARRAY_LEN_M(s, f) ((int64)(sizeof(((s*)0)->f) / sizeof((((s*)0)->f)[0])))

static void *(*lyc_malloc)(size_t size) = ::malloc;
static void *(*lyc_calloc)(size_t nmemb, size_t size) = ::calloc;
static void *(*lyc_realloc)(void *ptr, size_t size) = ::realloc;
static void (*lyc_free)(void *ptr) = ::free;

#define MALLOC(type, num) ((type *)(lyc_malloc((num) * sizeof(type))))
#define CALLOC(type, num) ((type *)(lyc_calloc((num), sizeof(type))))
#define REALLOC(ptr, type, num) ((type *)(lyc_realloc((ptr), (num) * sizeof(type))))
#define FREE(ptr) (lyc_free(ptr))

template <typename F>
struct ScopeExit {
  F func;
  ScopeExit(F f) : func(f) {}
  ~ScopeExit() { func(); }
};
template <typename F>
ScopeExit<F> make_scope_exit(F f) {
  return ScopeExit<F>(f);
}
#define DO_STRING_JOIN2(arg1, arg2) arg1##arg2
#define STRING_JOIN2(arg1, arg2) DO_STRING_JOIN2(arg1, arg2)
#define DEFER_COPY(code) auto STRING_JOIN2(scope_exit_, __LINE__) = make_scope_exit([=] { code; })
#define DEFER(code) auto STRING_JOIN2(scope_exit_, __LINE__) = make_scope_exit([&] { code; })

#define QUAD_VERTS_XYZ_ST(buf, x, y, z, w, h, s0, t0, s1, t1)                       \
  float buf[] = {x, y, z, s0, t0, x + w, y, z, s1, t0, x, y + h, z, s0, t1,         \
                 x, y + h, z, s0, t1, x + w, y, z, s1, t0, x + w, y + h, z, s1, t1};

#define QUAD_VERTS_XYZ_RGBA_ST(buf, x, y, z, w, h, r, g, b, a, s0, t0, s1, t1)                                           \
  float buf[] = {x, y, z, r, g, b, a, s0, t0, x + w, y, z, r, g, b, a, s1, t0, x, y + h, z, r, g, b, a, s0, t1,          \
                 x, y + h, z, r, g, b, a, s0, t1, x + w, y, z, r, g, b, a, s1, t0, x + w, y + h, z, r, g, b, a, s1, t1};

namespace { // simple math
template <typename T>
T max(T a, T b) {
  return (a < b) ? b : a;
}

template <typename T>
T min(T a, T b) {
  return (a < b) ? a : b;
}

#define IDENTITY_MAT(mat)                                           \
  float mat[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

#define ORTHO_PROJECTION_MAT(mat, left, right, bottom, top, far, near)                                                 \
  float mat[] = {2.0f / (right - left), 0, 0, 0, 0, 2.0f / (top - bottom), 0, 0, 0, 0, -2.0f / (far - near), 0,        \
                 -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1};
} // simple math

namespace { // simple dynamic string
#define STRING_INIT_CAPACITY 64u
#define STRING_GROWTH_FACTOR 2

struct StringHeader {
  uint32 len;
  uint32 free;
};

static void delete_str(char *str) {
  if (str) {
    FREE(str - sizeof(StringHeader));
  }
}

static uint32 str_len(const char *str) {
  if (str) {
    return ((StringHeader *)(str) - 1)->len;
  } else {
    return 0;
  }
}

static int str_cmp_impl(const char *str1, const char *str2, uint32 str2_len) {
  uint32 str1_len = str_len(str1);
  if (str1_len != str2_len) {
    return str1_len > str2_len ? 1 : -1;
  } else {
    return memcmp(str1, str2, str1_len);
  }
}

static int str_cmp(const char *str1, const char *str2) {
  return str_cmp_impl(str1, str2, str_len(str2));
}

static int str_cmp_c(const char *str1, const char *str2) {
  return str_cmp_impl(str1, str2, strlen(str2));
}

static void str_set_impl(char **str1, const char *str2, uint32 str2_len) {
  assert(str1);
  if (!*str1) {
    uint32 size = max((uint32)((sizeof(StringHeader) + str2_len + 1) * STRING_GROWTH_FACTOR), STRING_INIT_CAPACITY);
    char *new_str = MALLOC(char, size);
    ((StringHeader *)new_str)->len = str2_len;
    ((StringHeader *)new_str)->free = size - sizeof(StringHeader) - str2_len - 1;
    memcpy(new_str + sizeof(StringHeader), str2, str2_len + 1);
    *str1 = new_str + sizeof(StringHeader);
  } else {
    StringHeader *header = (StringHeader *)(*str1) - 1;
    uint32 capacity = header->len + 1 + header->free;
    if (capacity > str2_len) {
      header->len = str2_len;
      header->free = capacity - str2_len - 1;
      memcpy(*str1, str2, str2_len + 1);
    } else {
      uint32 new_size = (uint32)((sizeof(StringHeader) + str2_len + 1) * STRING_GROWTH_FACTOR);
      header = (StringHeader*)REALLOC(header, char, new_size);
      *str1 = (char*)(header + 1);
      header->len = str2_len;
      header->free = new_size - sizeof(StringHeader) - str2_len - 1;
      memcpy(*str1, str2, str2_len + 1);
    }
  }
}

static void str_set(char **str1, const char *str2) {
  str_set_impl(str1, str2, str_len(str2));
}

static void str_set_c(char **str1, const char *str2) {
  str_set_impl(str1, str2, strlen(str2));
}

static char* str_dup(const char *str) {
  if (str) {
    char *new_str = nullptr;
    str_set(&new_str, str);
    return new_str;
  } else {
    return nullptr;
  }
}

static char* str_dup_c(const char *str) {
  if (str) {
    char *new_str = nullptr;
    str_set_c(&new_str, str);
    return new_str;
  } else {
    return nullptr;
  }
}

static void str_cat(char **str, char c) {
  assert(str);
  if (!*str) {
    uint32 size = STRING_INIT_CAPACITY;
    char *new_str = MALLOC(char, size);
    ((StringHeader *)new_str)->len = 1;
    ((StringHeader *)new_str)->free = size - sizeof(StringHeader) - 1 - 1;
    *(new_str + sizeof(StringHeader)) = c;
    *(new_str + sizeof(StringHeader) + 1) = '\0';
    *str = new_str + sizeof(StringHeader);
  } else {
    StringHeader *header = (StringHeader *)(*str) - 1;
    if (header->free < 1) {
      uint32 new_size = (uint32)((sizeof(StringHeader) + header->len + 1 + 1) * STRING_GROWTH_FACTOR);
      header = (StringHeader *)REALLOC(header, char, new_size);
      header->free = new_size - sizeof(StringHeader) - header->len - 1;
      *str = (char*)(header + 1);
    }
    header->len += 1;
    header->free -= 1;
    *(*str + header->len - 1) = c;
    *(*str + header->len) = '\0';
  }
}

static void str_cat_impl(char **str1, const char *str2, uint32 str2_len) {
  assert(str1);
  if (!*str1) {
    uint32 size = max((uint32)((sizeof(StringHeader) + str2_len + 1) * STRING_GROWTH_FACTOR), STRING_INIT_CAPACITY);
    char *new_str = MALLOC(char, size);
    ((StringHeader *)new_str)->len = str2_len;
    ((StringHeader *)new_str)->free = size - sizeof(StringHeader) - str2_len - 1;
    memcpy(new_str + sizeof(StringHeader), str2, str2_len + 1);
    *str1 = new_str + sizeof(StringHeader);
  } else {
    StringHeader *header = (StringHeader *)(*str1) - 1;
    if (header->free < str2_len) {
      uint32 new_size = (uint32)((sizeof(StringHeader) + header->len + 1 + str2_len) * STRING_GROWTH_FACTOR);
      header = (StringHeader *)REALLOC(header, char, new_size);
      header->free = new_size - sizeof(StringHeader) - header->len - 1;
      *str1 = (char*)(header + 1);
    }
    header->len += str2_len;
    header->free -= str2_len;
    memcpy(*str1 + header->len - str2_len, str2, str2_len + 1);
  }
}

static void str_cat(char **str1, const char *str2) {
  str_cat_impl(str1, str2, str_len(str2));
}

static void str_cat_c(char **str1, const char *str2) {
  str_cat_impl(str1, str2, strlen(str2));
}

static void str_pop(char *str, uint32 n) {
  assert(str);
  StringHeader *header = (StringHeader *)(str) - 1;
  assert(header->len >= n);
  header->len -= n;
  header->free += n;
  str[header->len] = '\0';
}

static void str_pop_to_char(char *str, char c) {
  assert(str);
  StringHeader *header = (StringHeader *)(str) - 1;
  if (header->len > 1) {
    char *end = str + header->len - 1;
    while (*end != c && end != str) {
      --end;
    }
    if (end != str) {
      *(end + 1) = '\0';
      uint32 old_len = header->len;
      header->len = (end + 1) - str;
      header->free += (old_len - header->len);
    }
  }
}

static void str_replace(char *str, char a, char b) {
  uint32 len = str_len(str);
  for (int i = 0; i < len; ++i) {
    if (str[i] == a) {
      str[i] = b;
    }
  }
}
} // simple dynamic string

struct Program { // root data structure of the entire program
#ifdef __ANDROID__
  android_app* app;
#endif
  struct OpenGLES {
#ifdef __ANDROID__
    EGLDisplay display;
    EGLConfig display_config;
    EGLSurface surface;
    EGLContext context;
#endif
    int surface_width;
    int surface_height;
    struct Shaders {
      struct Text {
        GLuint id;
        GLint uniform_mvp_mat;
        GLint uniform_tex;
        GLuint attrib_position;
        GLuint attrib_color;
        GLuint attrib_tcoord;
      } text;
    } shaders;
  } opengl_es;
  struct Font {
    stbtt_fontinfo info;
    float scale_factor;
    byte *atlas;
    int atlas_w;
    int atlas_h;
    stbtt_packedchar *packed_chars; // currently always ascii 32 to 126
    int num_packed_chars;
    GLuint gl_texture_id;
  } font;
  struct KeyboardState {
    bool shift_on;
  } keyboard_state;
  struct OnScreenText{
    char *text;
    GLuint gl_verts_buf_id; // xyz,rgba,st
    uint32 gl_verts_buf_size;
    uint32 gl_verts_buf_size_in_use;
    float pen_pos_x;
    float pen_pos_y;
  } on_screen_text;
};

static void gl_swap_buffers(Program::OpenGLES *opengl_es) {
#ifdef __ANDROID__
  eglSwapBuffers(opengl_es->display, opengl_es->surface);
#endif
#ifdef __APPLE__
  EAGLContext* context = [EAGLContext currentContext];
  [context presentRenderbuffer:GL_RENDERBUFFER];
#endif
}

static void init_opengl_es_shaders(Program::OpenGLES::Shaders *shaders) {
  struct ProgramSrc {
    const char *vert_shader;
    int vert_shader_len;
    const char *frag_shader;
    int frag_shader_len;
  };
  auto split_src_string = [](const char *src) -> ProgramSrc {
    #define VS_STAGE_STR "[Vertex Shader Stage]"
    #define FS_STAGE_STR "[Fragment Shader Stage]"
    ProgramSrc psrc = {};
    const char *vsi_pos = strstr(src, VS_STAGE_STR);
    const char *fsi_pos = strstr(src, FS_STAGE_STR);
    if (vsi_pos && fsi_pos) {
      psrc.vert_shader = vsi_pos + strlen(VS_STAGE_STR);
      psrc.vert_shader_len = fsi_pos - psrc.vert_shader;
      psrc.frag_shader = fsi_pos + strlen(FS_STAGE_STR);
      psrc.frag_shader_len = strlen(src) - (psrc.frag_shader - src);
    }
    return psrc;
  };
  auto compile_shader = [](const char* shader_name, const char *src, int src_len, GLenum shader_type) -> GLuint {
    GLuint shader_id = glCreateShader(shader_type);
    if (shader_id == 0) {
      return 0;
    }
    glShaderSource(shader_id, 1, &src, &src_len);
    glCompileShader(shader_id);
    GLint logLength;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
      GLchar *log = MALLOC(GLchar, logLength);
      DEFER(FREE(log));
      glGetShaderInfoLog(shader_id, logLength, &logLength, log);
      if (strcmp(log, "")) {
        LOGI("shader \"%s\" compile message:\n%s\n", shader_name, log);
      }
    }
    GLint status;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      glDeleteShader(shader_id);
      return 0;
    }
    return shader_id;
  };
  auto link_shader = [](const char* shader_name, GLuint vs_id, GLuint fs_id) -> GLuint {
    GLuint shader_id = glCreateProgram();
    if (!shader_id) {
      return 0;
    }
    glAttachShader(shader_id, vs_id);
    glAttachShader(shader_id, fs_id);
    glLinkProgram(shader_id);
    GLint logLength;
    glGetProgramiv(shader_id, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 0) {
      GLchar *log = MALLOC(GLchar, logLength);
      DEFER(FREE(log));
      glGetProgramInfoLog(shader_id, logLength, &logLength, log);
      if (strcmp(log, "")) {
        LOGI("shader \"%s\" link message:\n%s\n", shader_name, log);
      }
    }
    GLint status;
    glGetProgramiv(shader_id, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
      glDeleteProgram(shader_id);
      glDeleteShader(vs_id);
      glDeleteShader(fs_id);
      return 0;
    }
    glDetachShader(shader_id, vs_id);
    glDetachShader(shader_id, fs_id);
    glDeleteShader(vs_id);
    glDeleteShader(fs_id);
    return shader_id;
  };
  auto create_shader = [&](const char* shader_name, const char *src) -> GLuint {
    ProgramSrc psrc = split_src_string(src);
    GLuint vs_id = compile_shader(shader_name, psrc.vert_shader, psrc.vert_shader_len, GL_VERTEX_SHADER);
    GLuint fs_id = compile_shader(shader_name, psrc.frag_shader, psrc.frag_shader_len, GL_FRAGMENT_SHADER);
    GLuint shader_id = link_shader(shader_name, vs_id, fs_id);
    return shader_id;
  };
  { // text shader
    const char *src = R"(
      [Vertex Shader Stage]
      uniform mat4 mvp_mat;
      attribute vec3 position;
      attribute vec4 color;
      attribute vec2 tcoord;
      varying vec4 color_vs;
      varying vec2 tcoord_vs;
      void main() {
        color_vs = color;
        tcoord_vs = tcoord;
        gl_Position = mvp_mat * vec4(position, 1.0);
      }
      [Fragment Shader Stage]
      uniform sampler2D tex;
      varying mediump vec4 color_vs;
      varying mediump vec2 tcoord_vs;
      void main() {
        gl_FragColor.rgb = color_vs.rgb;
        gl_FragColor.a = texture2D(tex, tcoord_vs).a;
      }
    )";
    auto *shader = &shaders->text;
    shader->id = create_shader("simple_texture", src);;
    shader->uniform_mvp_mat = glGetUniformLocation(shader->id, "mvp_mat");
    shader->uniform_tex = glGetUniformLocation(shader->id, "tex");
    shader->attrib_position = glGetAttribLocation(shader->id, "position");
    shader->attrib_color = glGetAttribLocation(shader->id, "color");
    shader->attrib_tcoord = glGetAttribLocation(shader->id, "tcoord");
    LOGI("loaded \"simple texture\" shader(id:%d)", shader->id);
  }
}

static bool init_font_data(Program::Font *font, byte *font_buf) {
  if (!stbtt_InitFont(&font->info, font_buf, 0)) {
    LOGF("cannot init font info from font file");
    return false;
  }
  font->scale_factor = stbtt_ScaleForPixelHeight(&font->info, 64);
  stbtt_pack_context spc;
  byte *pixels = MALLOC(byte, 1024 * 1024);
  if (!stbtt_PackBegin(&spc, pixels, 1024, 1024, 0, 0, nullptr)) {
    LOGF("stb_truetype PackBegin error");
    return false;
  }
  stbtt_PackSetOversampling(&spc, 2, 2);
  stbtt_packedchar *packed_chars = MALLOC(stbtt_packedchar, 95);
  if (!stbtt_PackFontRange(&spc, font_buf, 0, 64, 32, 95, packed_chars)) {
    LOGF("stb_truetype PackFontRange error");
    return false;
  }
  stbtt_PackEnd(&spc);

  font->atlas = pixels;
  font->atlas_w = 1024;
  font->atlas_h = 1024;
  font->packed_chars = packed_chars;
  font->num_packed_chars = 95;
  return true;
}

static void init_font_atlas_texture(Program::Font *font) {
  glGenTextures(1, &font->gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, font->gl_texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font->atlas_w, font->atlas_h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, font->atlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

static void add_char_to_on_screen_text_verts_buf(Program *program, char c) {
  auto *text = &program->on_screen_text;
  if (text->gl_verts_buf_id == 0) {
    glGenBuffers(1, &text->gl_verts_buf_id);
    glBindBuffer(GL_ARRAY_BUFFER, text->gl_verts_buf_id);
    glBufferData(GL_ARRAY_BUFFER, 1024 * 16, nullptr, GL_STATIC_DRAW);
    text->gl_verts_buf_size = 1024 * 16;
    text->gl_verts_buf_size_in_use = 0;
  }
  int c_size = 36 * 6;
  if ((text->gl_verts_buf_size - text->gl_verts_buf_size_in_use) < c_size) {
    uint32 new_size = text->gl_verts_buf_size * 2;
    GLuint new_buf;
    glGenBuffers(1, &new_buf);
    glBindBuffer(GL_COPY_WRITE_BUFFER, new_buf);
    glBufferData(GL_COPY_WRITE_BUFFER, new_size, nullptr, GL_STATIC_COPY);
    glBindBuffer(GL_COPY_READ_BUFFER, text->gl_verts_buf_id);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, text->gl_verts_buf_size);
    glDeleteBuffers(1, &text->gl_verts_buf_id);
    text->gl_verts_buf_id = new_buf;
    text->gl_verts_buf_size = new_size;
    LOGI("new size %d", text->gl_verts_buf_size);
  }
  auto *font_data = &program->font;
  int ascent, descent, line_gap;
  stbtt_GetFontVMetrics(&font_data->info, &ascent, &descent, &line_gap);
  float scale = font_data->scale_factor;
  ascent *= scale;
  descent *= scale;
  line_gap *= scale;
  int vertical_advance = ascent - descent + line_gap;
  int surface_width = program->opengl_es.surface_width;

  glBindBuffer(GL_ARRAY_BUFFER, text->gl_verts_buf_id);
  byte* buf_ptr = (byte *)glMapBufferRange(GL_ARRAY_BUFFER, text->gl_verts_buf_size_in_use, c_size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
  DEFER(glUnmapBuffer(GL_ARRAY_BUFFER));

  stbtt_aligned_quad quad;
  stbtt_GetPackedQuad(font_data->packed_chars, font_data->atlas_w, font_data->atlas_h, c - ' ', &text->pen_pos_x, &text->pen_pos_y, &quad, 0);
  float width = quad.x1 - quad.x0;
  float height = quad.y1 - quad.y0;
  QUAD_VERTS_XYZ_RGBA_ST(quad_verts_buf, quad.x0, quad.y1, 0, width, -height, 0, 1, 0, 1, quad.s0, quad.t1, quad.s1, quad.t0);
  memcpy(buf_ptr, quad_verts_buf, sizeof(quad_verts_buf));
  text->gl_verts_buf_size_in_use += c_size;
}

static void render_font_atlas(Program *program) {
  auto *font = &program->font;
  auto *shader = &program->opengl_es.shaders.text;
  glUseProgram(shader->id);
  ORTHO_PROJECTION_MAT(ortho_mat, -1, 1, 1, -1, -1, 1);
  glUniformMatrix4fv(shader->uniform_mvp_mat, 1, GL_FALSE, ortho_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font->gl_texture_id);
  glUniform1i(shader->uniform_tex, 0);

  QUAD_VERTS_XYZ_RGBA_ST(verts_buf, -1, -1, 0, 2, 2, 0, 1, 0, 1, 0, 0, 1, 1);
  GLuint gl_verts_buf;
  glGenBuffers(1, &gl_verts_buf);
  glBindBuffer(GL_ARRAY_BUFFER, gl_verts_buf);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts_buf), verts_buf, GL_STATIC_DRAW);

  glEnableVertexAttribArray(shader->attrib_position);
  glEnableVertexAttribArray(shader->attrib_color);
  glEnableVertexAttribArray(shader->attrib_tcoord);
  glVertexAttribPointer(shader->attrib_position, 3, GL_FLOAT, GL_FALSE, 36, (void *)0);
  glVertexAttribPointer(shader->attrib_color, 4, GL_FLOAT, GL_FALSE, 36, (void *)12);
  glVertexAttribPointer(shader->attrib_tcoord, 2, GL_FLOAT, GL_FALSE, 36, (void *)28);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
  int dh = (program->opengl_es.surface_height - program->opengl_es.surface_width) / 2;
  glViewport(0, dh, program->opengl_es.surface_width, program->opengl_es.surface_width);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  gl_swap_buffers(&program->opengl_es);
  glDisableVertexAttribArray(shader->attrib_position);
  glDisableVertexAttribArray(shader->attrib_color);
  glDisableVertexAttribArray(shader->attrib_tcoord);
  glDisable(GL_BLEND);
}

static void render_on_screen_text(Program *program) {
  auto *font = &program->font;
  auto *text = &program->on_screen_text;
  auto *shader = &program->opengl_es.shaders.text;
  int width = program->opengl_es.surface_width;
  int height = program->opengl_es.surface_height;
  glUseProgram(shader->id);
  ORTHO_PROJECTION_MAT(ortho_mat, 0, (float)width, (float)height, 0, -1, 1);
  glUniformMatrix4fv(shader->uniform_mvp_mat, 1, GL_FALSE, ortho_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font->gl_texture_id);
  glUniform1i(shader->uniform_tex, 0);

  glBindBuffer(GL_ARRAY_BUFFER, text->gl_verts_buf_id);
  glEnableVertexAttribArray(shader->attrib_position);
  glEnableVertexAttribArray(shader->attrib_color);
  glEnableVertexAttribArray(shader->attrib_tcoord);
  glVertexAttribPointer(shader->attrib_position, 3, GL_FLOAT, GL_FALSE, 36, (void *)0);
  glVertexAttribPointer(shader->attrib_color, 4, GL_FLOAT, GL_FALSE, 36, (void *)12);
  glVertexAttribPointer(shader->attrib_tcoord, 2, GL_FLOAT, GL_FALSE, 36, (void *)28);

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
  glDrawArrays(GL_TRIANGLES, 0, text->gl_verts_buf_size_in_use / 36);
  gl_swap_buffers(&program->opengl_es);
  glDisableVertexAttribArray(shader->attrib_position);
  glDisableVertexAttribArray(shader->attrib_color);
  glDisableVertexAttribArray(shader->attrib_tcoord);
  glDisable(GL_BLEND);
}
