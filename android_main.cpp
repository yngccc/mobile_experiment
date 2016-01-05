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

#include "shared.cpp"

bool init_opengl_es_display(Program *program) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) {
    LOGW("cannot eglGetDisplay");
    return false;
  }
  if (eglInitialize(display, nullptr, nullptr) == EGL_FALSE) {
    LOGW("cannot eglInitialize");
    return false;
  }
  EGLint display_attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
  EGLConfig display_config;
  EGLint num_config;
  eglChooseConfig(display, display_attribs, &display_config, 1, &num_config);
  EGLint format;
  eglGetConfigAttrib(display, display_config, EGL_NATIVE_VISUAL_ID, &format);
  ANativeWindow_setBuffersGeometry(program->app->window, 0, 0, format);
  program->opengl_es.display = display;
  program->opengl_es.display_config = display_config;
  return true;
}

bool init_opengl_es_surface(Program* program) {
  EGLSurface surface = eglCreateWindowSurface(program->opengl_es.display, program->opengl_es.display_config, program->app->window, nullptr);
  if (surface == EGL_NO_SURFACE) {
    LOGW("cannot eglCreateWindowSurface");
    return false;
  }
  program->opengl_es.surface = surface;
  eglQuerySurface(program->opengl_es.display, surface, EGL_WIDTH, &program->opengl_es.surface_width);
  eglQuerySurface(program->opengl_es.display, surface, EGL_HEIGHT, &program->opengl_es.surface_height);
  return true;
}

bool init_opengl_es_context(Program* program) {
  EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
  EGLContext context = eglCreateContext(program->opengl_es.display, program->opengl_es.display_config, nullptr, context_attribs);
  if (!context) {
    LOGW("cannot eglCreateContext");
    return false;
  }
  program->opengl_es.context = context;
  return true;
}

bool init_opengl_es_everything(Program *program) {
  auto *gl = &program->opengl_es;
  if (!init_opengl_es_display(program) ||
      !init_opengl_es_surface(program) ||
      !init_opengl_es_context(program) ||
      eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context) == EGL_FALSE ||
      eglSwapBuffers(gl->display, gl->surface) == EGL_FALSE) {
    return false;
  }
  init_opengl_es_shaders(&gl->shaders);
  init_font_atlas_texture(&program->font);
  return true;
}

bool ensure_opengl_es_is_working(Program *program) {
  auto *gl = &program->opengl_es;
  if (gl->display == EGL_NO_DISPLAY) {
    return init_opengl_es_everything(program);
  } else {
    eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context);
    if (eglSwapBuffers(gl->display, gl->surface) != EGL_FALSE) {
      return true;
    } else {
      EGLint err = eglGetError();
      if (err == EGL_BAD_DISPLAY || err == EGL_BAD_CONTEXT || err == EGL_CONTEXT_LOST) {
        eglDestroyContext(gl->display, gl->context);
        eglDestroySurface(gl->display, gl->surface);
        eglTerminate(gl->display);
        return init_opengl_es_everything(program);
      } else if (err == EGL_BAD_SURFACE) {
        eglDestroySurface(gl->display, gl->surface);
        return init_opengl_es_surface(program) &&
               eglMakeCurrent(gl->display, gl->surface, gl->surface, gl->context) != EGL_FALSE &&
               eglSwapBuffers(gl->display, gl->surface) != EGL_FALSE;
      } else {
        return false;
      }
    }
  }
}

bool show_soft_keyboard(android_app *app, bool show) {
  // Java version
  //
  // InputMethodManager input_method = native_activity.getSystemService(Context.INPUT_METHOD_SERVICE)
  // Window window = native_activity.getWindow();
  // View view = window.getDecorView();
  // IBinder token = view.getWindowToken();
  // if (show) {
  //   return input_method.showSoftInput(view, 0);
  // } else {
  //   return input_method.hideSoftInputFromWindow(token, 0);
  // }

  JavaVM *java_vm = app->activity->vm;
  JNIEnv *jni_env = app->activity->env;
  JavaVMAttachArgs java_vm_attach_args = {JNI_VERSION_1_6, "NativeThread", nullptr};
  if (java_vm->AttachCurrentThread(&jni_env, &java_vm_attach_args) == JNI_ERR) {
    return false;
  }
  DEFER(java_vm->DetachCurrentThread());

  jclass native_activity_class  = jni_env->GetObjectClass(app->activity->clazz);
  jstring input_method_str = jni_env->NewStringUTF("input_method");
  jobject input_method = jni_env->CallObjectMethod(app->activity->clazz, jni_env->GetMethodID(native_activity_class, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;"), input_method_str);
  jclass input_method_class = jni_env->GetObjectClass(input_method);
  jobject window = jni_env->CallObjectMethod(app->activity->clazz, jni_env->GetMethodID(native_activity_class, "getWindow", "()Landroid/view/Window;"));
  jclass window_class = jni_env->GetObjectClass(window);
  jobject view = jni_env->CallObjectMethod(window, jni_env->GetMethodID(window_class, "getDecorView", "()Landroid/view/View;"));
  jclass view_class = jni_env->GetObjectClass(view);
  jobject token = jni_env->CallObjectMethod(view, jni_env->GetMethodID(view_class, "getWindowToken", "()Landroid/os/IBinder;"));
  DEFER(jni_env->DeleteLocalRef(native_activity_class); // are these DeleteLocalRefs necessary??
        jni_env->DeleteLocalRef(input_method_str);
        jni_env->DeleteLocalRef(input_method);
        jni_env->DeleteLocalRef(input_method_class);
        jni_env->DeleteLocalRef(window);
        jni_env->DeleteLocalRef(window_class);
        jni_env->DeleteLocalRef(view);
        jni_env->DeleteLocalRef(view_class);
        jni_env->DeleteLocalRef(token));
  if (show) {
    return jni_env->CallBooleanMethod(input_method, jni_env->GetMethodID(input_method_class, "showSoftInput", "(Landroid/view/View;I)Z"), view, 0);
  } else {
    return jni_env->CallBooleanMethod(input_method, jni_env->GetMethodID(input_method_class, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z"), token, 0);
  }
}

void handle_app_cmd(android_app* app, int32 cmd) {
  Program* program = (Program*)app->userData;
  switch (cmd) {
  case APP_CMD_INPUT_CHANGED: {
    LOGD("received APP_CMD_INPUT_CHANGED");
  } break;
  case APP_CMD_INIT_WINDOW: {
    LOGD("received APP_CMD_INIT_WINDOW");
    if (!ensure_opengl_es_is_working(program)) {
      LOGF("opengl es cannot be initialized or restored");
      exit(1);
    }
    render_font_atlas(program);
  } break;
  case APP_CMD_TERM_WINDOW: {
    LOGD("received APP_CMD_TERM_WINDOW");
  } break;
  case APP_CMD_WINDOW_RESIZED: {
    LOGD("received APP_CMD_WINDOW_RESIZED");
  } break;
  case APP_CMD_WINDOW_REDRAW_NEEDED: {
    LOGD("received APP_CMD_WINDOW_REDRAW_NEEDED");
  } break;
  case APP_CMD_CONTENT_RECT_CHANGED: {
    LOGD("received APP_CMD_CONTENT_RECT_CHANGED");
  } break;
  case APP_CMD_GAINED_FOCUS: {
    LOGD("received APP_CMD_GAINED_FOCUS");
  } break;
  case APP_CMD_LOST_FOCUS: {
    LOGD("received APP_CMD_LOST_FOCUS");
  } break;
  case APP_CMD_CONFIG_CHANGED: {
    LOGD("received APP_CMD_CONFIG_CHANGED");
  } break;
  case APP_CMD_LOW_MEMORY: {
    LOGD("received APP_CMD_LOW_MEMORY");
  } break;
  case APP_CMD_START: {
    program->keyboard_state.shift_on = false;
    int ascent;
    stbtt_GetFontVMetrics(&program->font.info, &ascent, nullptr, nullptr);
    program->on_screen_text.pen_pos_y = ascent * program->font.scale_factor + 250;
    LOGD("received APP_CMD_START");
  } break;
  case APP_CMD_RESUME: {
    LOGD("received APP_CMD_RESUME");
  } break;
  case APP_CMD_SAVE_STATE: {
    LOGD("received APP_CMD_SAVE_STATE");
  } break;
  case APP_CMD_PAUSE: {
    LOGD("received APP_CMD_PAUSE");
  } break;
  case APP_CMD_STOP: {
    delete_str(program->on_screen_text.text);
    program->on_screen_text = {};
    LOGD("received APP_CMD_STOP");
  } break;
  case APP_CMD_DESTROY: {
    LOGD("received APP_CMD_DESTROY");
  } break;
  }
}

char translate_keycode(int android_keycode, bool shift_on) {
  struct AlphabetKeymap {
    int32 android_key_code;
    int32 key;
  };
  struct SymbolKeymap {
    int32 android_key_code;
    int32 key;
    int32 key_shifted;
  };
  AlphabetKeymap alphabet_key_map[] = {
    {AKEYCODE_A, 'a'}, {AKEYCODE_B, 'b'}, {AKEYCODE_C, 'c'}, {AKEYCODE_D, 'd'}, {AKEYCODE_E, 'e'}, {AKEYCODE_F, 'f'},
    {AKEYCODE_G, 'g'}, {AKEYCODE_H, 'h'}, {AKEYCODE_I, 'i'}, {AKEYCODE_J, 'j'}, {AKEYCODE_K, 'k'}, {AKEYCODE_L, 'l'},
    {AKEYCODE_M, 'm'}, {AKEYCODE_N, 'n'}, {AKEYCODE_O, 'o'}, {AKEYCODE_P, 'p'}, {AKEYCODE_Q, 'q'}, {AKEYCODE_R, 'r'},
    {AKEYCODE_S, 's'}, {AKEYCODE_T, 't'}, {AKEYCODE_U, 'u'}, {AKEYCODE_V, 'v'}, {AKEYCODE_W, 'w'}, {AKEYCODE_X, 'x'},
    {AKEYCODE_Y, 'y'}, {AKEYCODE_Z, 'z'}};
  SymbolKeymap symbol_key_map[] = {
    {AKEYCODE_1, '1', '!'}, {AKEYCODE_2, '2', '@'}, {AKEYCODE_3, '3', '#'}, {AKEYCODE_4, '4', '$'}, {AKEYCODE_5, '5', '%'},
    {AKEYCODE_6, '6', '^'}, {AKEYCODE_7, '7', '&'}, {AKEYCODE_8, '8', '*'}, {AKEYCODE_9, '9', '('}, {AKEYCODE_0, '0', ')'},
    {AKEYCODE_SPACE, ' ', ' '}, {AKEYCODE_GRAVE, '`', '~'}, {AKEYCODE_MINUS, '-', '_'}, {AKEYCODE_EQUALS, '=', '+'},
    {AKEYCODE_LEFT_BRACKET, '[', '{'}, {AKEYCODE_RIGHT_BRACKET, ']', '}'}, {AKEYCODE_BACKSLASH, '\\', '|'},
    {AKEYCODE_SEMICOLON, ';', ':'}, {AKEYCODE_APOSTROPHE, '\'', '\"'},
    {AKEYCODE_COMMA, ',', '<'}, {AKEYCODE_PERIOD, '.', '>'}, {AKEYCODE_SLASH, '/', '\?'}};
  for (int i = 0; i < ARRAY_LEN(alphabet_key_map); ++i) {
    AlphabetKeymap *map = &alphabet_key_map[i];
    if (map->android_key_code == android_keycode) {
      return shift_on ? map->key - 32 : map->key;
    }
  }
  for (int i = 0; i < ARRAY_LEN(symbol_key_map); ++i) {
    SymbolKeymap *map = &symbol_key_map[i];
    if (map->android_key_code == android_keycode) {
      return shift_on ? map->key_shifted : map->key;
    }
  }
  return 0;
}

int32 handle_app_input(android_app* app, AInputEvent* event) {
  Program* program = (Program*)app->userData;
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
    int action = AKeyEvent_getAction(event);
    int keycode = AKeyEvent_getKeyCode(event);
    int repeat_count = AKeyEvent_getRepeatCount(event);
    if (action == AKEY_EVENT_ACTION_DOWN) {
      if (keycode == AKEYCODE_SHIFT_LEFT || keycode == AKEYCODE_SHIFT_RIGHT) {
        program->keyboard_state.shift_on = true;
      }
    } else if (action == AKEY_EVENT_ACTION_UP) {
      if (keycode == AKEYCODE_SHIFT_LEFT || keycode == AKEYCODE_SHIFT_RIGHT) {
        program->keyboard_state.shift_on = false;
      } else {
        char c = translate_keycode(keycode, program->keyboard_state.shift_on);
        if (c) {
          str_cat(&program->on_screen_text.text, c);
          add_char_to_on_screen_text_verts_buf(program, c);
          render_on_screen_text(program);
        }
      }
    } else if (action == AKEY_EVENT_ACTION_MULTIPLE) {
      LOGD("key multiple %d", keycode);
    }
  } else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    if (AMotionEvent_getAction(event) == AMOTION_EVENT_ACTION_DOWN) {
      static bool show = false;
      show = !show;
      show_soft_keyboard(app, show);
      return 1;
    } else if (AMotionEvent_getAction(event) == AMOTION_EVENT_ACTION_UP) {
      return 1;
    }
  }
  return 0;
}

void android_main(android_app* app) {
  app_dummy(); // app crashes without this, wtf?
  Program *program = CALLOC(Program, 1);
  program->app = app;
  program->opengl_es.display = EGL_NO_DISPLAY;
  app->userData = program;
  app->onAppCmd = handle_app_cmd;
  app->onInputEvent = handle_app_input;
  { // load font
    AAsset *asset = AAssetManager_open(app->activity->assetManager, "fonts/open-sans/OpenSans-Regular.ttf", AASSET_MODE_STREAMING);
    if (!asset) {
      LOGF("font file not found");
      return;
    }
    DEFER(AAsset_close(asset));
    int buf_len = AAsset_getLength(asset);
    byte *buf = MALLOC(byte, buf_len);
    if (AAsset_read(asset, buf, buf_len) != buf_len) {
      LOGF("cannot read font file");
      return;
    }
    if (!init_font(&program->font, buf)) {
      return;
    }
  }
  {
    if (pipe(program->network.dns_lookup_pipe) == -1) {
      LOGD("cannot create dns lookup pipe");
      return;
    }
    ALooper_addFd(app->looper, program->network.dns_lookup_pipe[0], 0, ALOOPER_EVENT_INPUT, dns_lookup_callback, program);
    Program::Network::DNSLookup *lookups[10];
    for (int i = 0; i < 10; ++i) {
      lookups[i] = MALLOC(Program::Network::DNSLookup, 1);
      lookups[i]->write_pipe = program->network.dns_lookup_pipe[1];
      lookups[i]->service = "80";
      lookups[i]->request = {};
      lookups[i]->request.ai_family = AF_UNSPEC;
      lookups[i]->request.ai_socktype = SOCK_STREAM;
      lookups[i]->request.ai_protocol = IPPROTO_TCP;
      lookups[i]->request.ai_flags = AI_ADDRCONFIG;
    }
    for (int i = 0; i < 9; ++i) {
      lookups[i]->next = lookups[i + 1];
    }
    lookups[9]->next = nullptr;
    lookups[0]->domain_name = "www.google.com";
    lookups[1]->domain_name = "www.facebook.com";
    lookups[2]->domain_name = "www.youtube.com";
    lookups[3]->domain_name = "www.baidu.com";
    lookups[4]->domain_name = "www.yahoo.com";
    lookups[5]->domain_name = "www.amazon.com";
    lookups[6]->domain_name = "www.wikipedia.org";
    lookups[7]->domain_name = "www.qq.com";
    lookups[8]->domain_name = "www.twitter.com";
    lookups[9]->domain_name = "www.taobao.com";
    dns_lookup(&program->network, lookups[0]);
  }
  for (;;) { // event loop
    int fd;
    int event;
    android_poll_source* source;
    int fd_type = ALooper_pollAll(-1, &fd, &event, (void**)&source);
    if (fd_type >= 0) {
      if (app->destroyRequested) {
        break;
      }
      if (source) {
        source->process(app, source);
      }
    } else if (fd_type == ALOOPER_POLL_TIMEOUT) {
      LOGD("time out: %d", fd_type);
    }
  }
  return;
}
