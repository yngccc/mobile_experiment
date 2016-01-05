/* Copyright (C) 2015-2016 yang chen yngccc@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. */

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

#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "yngccc", __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "yngccc", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "yngccc", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "yngccc", __VA_ARGS__))
#define LOGF(...) ((void)__android_log_print(ANDROID_LOG_FATAL, "yngccc", __VA_ARGS__))
#endif

#ifdef __APPLE__
#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>

#define LOGD(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGI(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGW(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGE(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#define LOGF(...) (fprintf(stderr, __VA_ARGS__), fprintf(stderr, "\n"))
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdatomic.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define HTTP_PARSER_IMPLEMENTATION
#include "http_parser.h"

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
#define STRING_GROWTH_FACTOR 2u

struct StringHeader {
  uint32 len;
  uint32 free;
};

void delete_str(char *str) {
  if (str) {
    FREE(str - sizeof(StringHeader));
  }
}

uint32 str_len(const char *str) {
  if (str) {
    return ((StringHeader *)(str) - 1)->len;
  } else {
    return 0;
  }
}

int str_cmp_impl(const char *str1, const char *str2, uint32 str2_len) {
  uint32 str1_len = str_len(str1);
  if (str1_len != str2_len) {
    return str1_len > str2_len ? 1 : -1;
  } else {
    return memcmp(str1, str2, str1_len);
  }
}

int str_cmp(const char *str1, const char *str2) {
  return str_cmp_impl(str1, str2, str_len(str2));
}

int str_cmp_c(const char *str1, const char *str2) {
  return str_cmp_impl(str1, str2, strlen(str2));
}

void str_set_impl(char **str1, const char *str2, uint32 str2_len) {
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

void str_set(char **str1, const char *str2) {
  str_set_impl(str1, str2, str_len(str2));
}

void str_set_c(char **str1, const char *str2) {
  str_set_impl(str1, str2, strlen(str2));
}

char* str_dup(const char *str) {
  if (str) {
    char *new_str = nullptr;
    str_set(&new_str, str);
    return new_str;
  } else {
    return nullptr;
  }
}

char* str_dup_c(const char *str) {
  if (str) {
    char *new_str = nullptr;
    str_set_c(&new_str, str);
    return new_str;
  } else {
    return nullptr;
  }
}

void str_cat(char **str, char c) {
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

void str_cat_impl(char **str1, const char *str2, uint32 str2_len) {
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

void str_cat(char **str1, const char *str2) {
  str_cat_impl(str1, str2, str_len(str2));
}

void str_cat_c(char **str1, const char *str2) {
  str_cat_impl(str1, str2, strlen(str2));
}

void str_pop(char *str, uint32 n) {
  assert(str);
  StringHeader *header = (StringHeader *)(str) - 1;
  assert(header->len >= n);
  header->len -= n;
  header->free += n;
  str[header->len] = '\0';
}

void str_pop_to_char(char *str, char c) {
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

void str_replace(char *str, char a, char b) {
  uint32 len = str_len(str);
  for (int i = 0; i < len; ++i) {
    if (str[i] == a) {
      str[i] = b;
    }
  }
}
} // simple dynamic string

namespace { // simple dynamic array
#define ARRAY_INIT_CAPACITY 64u
#define ARRAY_GROWTH_FACTOR 2u

struct ArrayHeader {
  uint32 size;
  uint32 free;
};

template <typename T>
void delete_array(T *array) {
  if (array) {
    FREE((char *)array - sizeof(ArrayHeader));
  }
}

template <typename T>
uint32 array_size(const T *array) {
  if (array) {
    ArrayHeader *header = (ArrayHeader *)((char *)array - sizeof(ArrayHeader));
    return header->size;
  } else {
    return 0;
  }
}

template <typename T>
ArrayHeader *array_header(const T *array) {
  assert(array);
  ArrayHeader *header = (ArrayHeader *)((char *)array - sizeof(ArrayHeader));
  return header;
}

template <typename T>
T array_last(const T *array) {
  uint32 size = array_size(array);
  assert(size > 0);
  return array[size - 1];
}

template <typename T>
void array_reserve(T **array, uint32 num_items) {
  assert(array);
  if (!*array) {
    char *new_array = MALLOC(char, sizeof(ArrayHeader) + num_items * sizeof(T));
    ArrayHeader *header = (ArrayHeader *)new_array;
    header->size = 0;
    header->free = num_items;
    *array = (T *)(new_array + sizeof(ArrayHeader));
  } else {
    ArrayHeader *header = (ArrayHeader *)((char *)(*array) - sizeof(ArrayHeader));
    if ((header->size + header->free) < num_items) {
      char *new_array = REALLOC(header, char, sizeof(ArrayHeader) + num_items * sizeof(T));
      header = (ArrayHeader *)new_array;
      *array = (T *)(new_array + sizeof(ArrayHeader));
      header->free = num_items - header->size;
    }
  }
}

template <typename T>
void array_resize(T **array, uint32 new_size) {
  assert(array);
  if (!*array) {
    uint32 capacity = new_size * ARRAY_GROWTH_FACTOR;
    char *new_array = CALLOC(char, sizeof(ArrayHeader) + capacity * sizeof(T));
    ArrayHeader *header = (ArrayHeader *)new_array;
    header->size = new_size;
    header->free = capacity - new_size;
    *array = (T *)(new_array + sizeof(ArrayHeader));
  } else {
    ArrayHeader *header = (ArrayHeader *)((char *)(*array) - sizeof(ArrayHeader));
    if (header->size >= new_size) {
      uint32 diff = header->size - new_size;
      header->size -= diff;
      header->free += diff;
    } else {
      uint32 capacity = header->size + header->free;
      if (new_size <= capacity) {
        uint32 diff = new_size - header->size;
        header->size += diff;
        header->free -= diff;
        memset(*array + (header->size - diff), 0, diff * sizeof(T));
      } else {
        uint32 new_capacity = new_size * ARRAY_GROWTH_FACTOR;
        char *new_array = REALLOC(header, char, sizeof(ArrayHeader) + new_capacity * sizeof(T));
        header = (ArrayHeader *)new_array;
        uint32 old_size = header->size;
        header->size = new_size;
        header->free = new_capacity - new_size;
        *array = (T *)(new_array + sizeof(ArrayHeader));
        memset(*array + old_size, 0, (new_size - old_size) * sizeof(T));
      }
    }
  }
}

template <typename T>
void array_set(T *array, T value) {
  for (uint32 i = 0; i < array_size(array); ++i) {
    array[i] = value;
  }
}

template <typename T>
void array_push(T **array, const T &item) {
  assert(array);
  if (!*array) {
    char *new_array = MALLOC(char, sizeof(ArrayHeader) + ARRAY_INIT_CAPACITY * sizeof(T));
    ArrayHeader *header = (ArrayHeader *)new_array;
    header->size = 1;
    header->free = ARRAY_INIT_CAPACITY - 1;
    *(T *)(new_array + sizeof(ArrayHeader)) = item;
    *array = (T *)(new_array + sizeof(ArrayHeader));
  } else {
    ArrayHeader *header = (ArrayHeader *)((char *)(*array) - sizeof(ArrayHeader));
    if (header->free == 0) {
      uint32 new_capacity = header->size * ARRAY_GROWTH_FACTOR;
      char *new_array = REALLOC(header, char, sizeof(ArrayHeader) + new_capacity * sizeof(T));
      header = (ArrayHeader *)new_array;
      header->free = new_capacity - header->size;
      *array = (T *)(new_array + sizeof(ArrayHeader));
    }
    header->size += 1;
    header->free -= 1;
    (*array)[header->size - 1] = item;
  }
}

template <typename T>
void array_push(T **array, const T *items, uint32 num_items) {
  assert(array);
  if (!*array) {
    uint32 capacity = max(ARRAY_INIT_CAPACITY, num_items * ARRAY_GROWTH_FACTOR);
    char *new_array = MALLOC(char, sizeof(ArrayHeader) + capacity * sizeof(T));
    ArrayHeader *header = (ArrayHeader *)new_array;
    header->size = num_items;
    header->free = ARRAY_INIT_CAPACITY - num_items;
    memcpy(new_array + sizeof(ArrayHeader), items, num_items * sizeof(T));
    *array = (T *)(new_array + sizeof(ArrayHeader));
  } else {
    ArrayHeader *header = (ArrayHeader *)((char *)(*array) - sizeof(ArrayHeader));
    if (header->free < num_items) {
      uint32 new_capacity = (header->size + num_items) * ARRAY_GROWTH_FACTOR;
      char *new_array = REALLOC(header, char, sizeof(ArrayHeader) + new_capacity * sizeof(T));
      header = (ArrayHeader *)new_array;
      header->free = new_capacity - header->size;
      *array = (T *)(new_array + sizeof(ArrayHeader));
    }
    header->size += num_items;
    header->free -= num_items;
    memcpy((*array) + (header->size - num_items), items, num_items * sizeof(T));
  }
}

template <typename T>
T array_pop(T *array) {
  assert(array);
  ArrayHeader *header = (ArrayHeader *)((char *)array - sizeof(ArrayHeader));
  assert(header->size > 0);
  header->size -= 1;
  header->free += 1;
  return array[header->size];
}

template <typename T>
void array_pop(T *array, uint32 num_items, T *items = nullptr) {
  assert(array);
  ArrayHeader *header = (ArrayHeader *)((char *)array - sizeof(ArrayHeader));
  assert(header->size >= num_items);
  header->size -= num_items;
  header->free += num_items;
  if (items) {
    memcpy(items, array + header->size, num_items * sizeof(T));
  }
}

template <typename T>
void array_clear(T *array) {
  if (array) {
    ArrayHeader *header = (ArrayHeader *)((char *)array - sizeof(ArrayHeader));
    header->free += header->size;
    header->size = 0;
  }
}

template <typename T>
T array_swap_with_end_then_pop(T *array, uint32 index) {
  ArrayHeader *header = (ArrayHeader *)((char *)array - sizeof(ArrayHeader));
  assert(header->size > 0);
  uint32 end = header->size - 1;
  assert(index <= end);
  --header->size;
  ++header->free;
  T value = array[index];
  if (index < end) {
    array[index] = array[end];
  }
  return value;
}
} // simple dynamic array

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
    stbtt_packedchar *packed_chars; // currently always ascii 32 to 126
    int num_packed_chars;
    byte *atlas;
    int atlas_w;
    int atlas_h;
    GLuint atlas_gl_texture_id;
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
  struct Network {
    int dns_lookup_pipe[2];
    struct DNSLookup {
      _Atomic int status; // 0:not started, 1:start failed 2:in progress, 3:succeed, 4:failed
      int write_pipe;
      const char *domain_name;
      const char *service;
      addrinfo request;
      addrinfo *response;
      DNSLookup *next;
    } *dns_lookups;
    struct ConnectionAttempt {
      int socket_fd;
      const char *domain_name;
      addrinfo *addr_list;
      addrinfo *cur_addr;
    } *connection_attempts;
    struct Connection {
      int socket_fd;
      const char *domain_name;
    } *connections;
  } network;
};

void gl_swap_buffers(Program::OpenGLES *opengl_es) {
  #ifdef __APPLE__
  [[EAGLContext currentContext] presentRenderbuffer:GL_RENDERBUFFER];
  #else
  eglSwapBuffers(opengl_es->display, opengl_es->surface);
  #endif
}

void init_opengl_es_shaders(Program::OpenGLES::Shaders *shaders) {
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

bool init_font(Program::Font *font, byte *font_buf) {
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

void init_font_atlas_texture(Program::Font *font) {
  glGenTextures(1, &font->atlas_gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, font->atlas_gl_texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font->atlas_w, font->atlas_h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, font->atlas);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void add_char_to_on_screen_text_verts_buf(Program *program, char c) {
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
    LOGI("on screen text verts buf new size %d", text->gl_verts_buf_size);
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

void render_font_atlas(Program *program) {
  auto *font = &program->font;
  auto *shader = &program->opengl_es.shaders.text;
  glUseProgram(shader->id);
  ORTHO_PROJECTION_MAT(ortho_mat, -1, 1, 1, -1, -1, 1);
  glUniformMatrix4fv(shader->uniform_mvp_mat, 1, GL_FALSE, ortho_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font->atlas_gl_texture_id);
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

void render_on_screen_text(Program *program) {
  auto *font = &program->font;
  auto *text = &program->on_screen_text;
  auto *shader = &program->opengl_es.shaders.text;
  int width = program->opengl_es.surface_width;
  int height = program->opengl_es.surface_height;
  glUseProgram(shader->id);
  ORTHO_PROJECTION_MAT(ortho_mat, 0, (float)width, (float)height, 0, -1, 1);
  glUniformMatrix4fv(shader->uniform_mvp_mat, 1, GL_FALSE, ortho_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font->atlas_gl_texture_id);
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

void *dns_lookup_proc(void *user_data) {
  auto *lookup = (Program::Network::DNSLookup *)user_data;
  assert(atomic_load(&lookup->status) == 0);
  atomic_store(&lookup->status, 2);
  int err = getaddrinfo(lookup->domain_name, lookup->service, &lookup->request, &lookup->response);
  if (err == 0) {
    atomic_store(&lookup->status, 3);
  } else {
    atomic_store(&lookup->status, 4);
  }
  write(lookup->write_pipe, &lookup, sizeof(void*));
  return nullptr;
}

void dns_lookup(Program::Network *network, Program::Network::DNSLookup *lookups) {
  Program::Network::DNSLookup *lookup = lookups;
  while (lookup) {
    atomic_store(&lookup->status, 0);
    pthread_t thread;
    if (pthread_create(&thread, nullptr, dns_lookup_proc, lookup)) {
      atomic_store(&lookup->status, 1);
    }
    pthread_detach(thread);
    lookup = lookup->next;
  }
  if (!network->dns_lookups) {
    network->dns_lookups = lookups;
  } else {
    Program::Network::DNSLookup *lookup = network->dns_lookups;
    while (lookup->next) {
      lookup = lookup->next;
    }
    lookup->next = lookups;
  }
}

void finish_connection_attempt(Program *program, int attempt_index);
int connection_getopt_callback(int fd, int events, void* data);
int connection_read_write_callback(int fd, int events, void* data);

#ifdef __ANDROID__

void start_connection_attempt(Program *program, int attempt_index) {
  ALooper *alooper = program->app->looper;
  Program::Network *network = &program->network;
  Program::Network::ConnectionAttempt *ca = &network->connection_attempts[attempt_index];
  ALooper_removeFd(alooper, ca->socket_fd);
  close(ca->socket_fd);
  while (ca->cur_addr) {
    LOGD("trying create socket with addr, domain name %s", ca->domain_name);
    int socket_fd = socket(ca->cur_addr->ai_family, ca->cur_addr->ai_socktype, ca->cur_addr->ai_protocol);
    if (socket_fd != -1 && fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) & ~O_NONBLOCK) != -1) {
      LOGD("successful create socket with addr, domain name %s", ca->domain_name);
      ca->socket_fd = socket_fd;
      break;
    }
    LOGD("failure create socket with addr, domain name %s", ca->domain_name);
    close(socket_fd);
    ca->cur_addr = ca->cur_addr->ai_next;
  }
  if (!ca->cur_addr) {
    LOGD("cannot create socket, domain name %s", ca->domain_name);
    freeaddrinfo(ca->addr_list);
    array_swap_with_end_then_pop(network->connection_attempts, attempt_index);
  } else {
    int err = connect(ca->socket_fd, ca->cur_addr->ai_addr, ca->cur_addr->ai_addrlen);
    if (!err) {
      LOGD("socket connected, domain name %s", ca->domain_name);
      finish_connection_attempt(program, attempt_index);
    } else if (errno == EINPROGRESS) {
      LOGD("socket connect in progress, domain name %s", ca->domain_name);
      ALooper_addFd(alooper, ca->socket_fd, 0, ALOOPER_EVENT_OUTPUT, connection_getopt_callback, program);
    } else {
      LOGD("oops unhandled socket error(%d)", err);
      exit(1);
    }
  }
}

void finish_connection_attempt(Program *program, int attempt_index) {
  ALooper *alooper = program->app->looper;
  Program::Network::ConnectionAttempt *ca = &program->network.connection_attempts[attempt_index];
  Program::Network::Connection conn = {};
  conn.socket_fd = ca->socket_fd;
  conn.domain_name = ca->domain_name;
  freeaddrinfo(ca->addr_list);
  array_swap_with_end_then_pop(program->network.connection_attempts, attempt_index);
  ALooper_addFd(alooper, conn.socket_fd, 0, ALOOPER_EVENT_INPUT | ALOOPER_EVENT_OUTPUT, connection_read_write_callback, program);
  array_push(&program->network.connections, conn);
}

int dns_lookup_callback(int fd, int event, void* data) {
  Program *program = (Program*)data;
  Program::Network::DNSLookup *lookups[32];
  int num_bytes = read(fd, lookups, sizeof(lookups));
  assert(num_bytes % sizeof(void*) == 0);
  int num_lookups = num_bytes / sizeof(void*);
  for (int i = 0; i < num_lookups; ++i) {
    int status = atomic_load(&lookups[i]->status);
    if (status == 4) {
      LOGD("cannot lookup dns, domain name: %s", lookups[i]->domain_name);
    } else if (status == 3) {
      Program::Network::ConnectionAttempt ca = {};
      ca.socket_fd = -1;
      ca.domain_name = lookups[i]->domain_name;
      ca.addr_list = lookups[i]->response;
      ca.cur_addr = ca.addr_list;
      array_push(&program->network.connection_attempts, ca);
      start_connection_attempt(program, array_size(program->network.connection_attempts) - 1);
    }
    // TODO: remove lookups[i] from program->network->dns_lookups
  }
  return 1;
}

int connection_getopt_callback(int fd, int event, void* data) {
  Program *program = (Program*)data;
  LOGD("connection getsockopt");
  int i;
  for (i = 0; i < array_size(program->network.connection_attempts); ++i) {
    if (program->network.connection_attempts[i].socket_fd == fd) {
      break;
    }
  }
  assert(i < array_size(program->network.connection_attempts));
  Program::Network::ConnectionAttempt ca = program->network.connection_attempts[i];
  int err;
  socklen_t err_len = sizeof(err);
  getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
  if (!err) {
    LOGD("connection getsockopt succeed, domain name: %s", ca.domain_name);
    finish_connection_attempt(program, i);
  } else {
    LOGD("connection getsockopt failed, try next addr, domain name: %s", ca.domain_name);
    ca.cur_addr = ca.cur_addr->ai_next;
    start_connection_attempt(program, i);
  }
  return 1;
}

int connection_read_write_callback(int fd, int event, void* data) {
  Program *program = (Program*)data;
  int i;
  for (i = 0; i < array_size(program->network.connections); ++i) {
    if (program->network.connections[i].socket_fd == fd) {
      break;
    }
  }
  assert(i < array_size(program->network.connections));
  Program::Network::Connection conn = program->network.connections[i];
  if (event == ALOOPER_EVENT_INPUT) {
    char response[2048];
    int n = read(conn.socket_fd, response, sizeof(response));
    response[n - 1] = '\0';
    LOGI("\n>>>>>>>>>>>>>%s<<<<<<<<<<<<<<<<\n%s", conn.domain_name, response);
    ALooper_removeFd(program->app->looper, conn.socket_fd);
    close(conn.socket_fd);
    array_swap_with_end_then_pop(program->network.connections, i);
  } else if (event == ALOOPER_EVENT_OUTPUT) {
    char request[256];
    snprintf(request, sizeof(request), "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", conn.domain_name);
    int n = write(conn.socket_fd, request, strlen(request));
    ALooper_addFd(program->app->looper, conn.socket_fd, 0, ALOOPER_EVENT_INPUT, connection_read_write_callback, program);
  }
  return 1;
}

#endif // __ANDROID__
