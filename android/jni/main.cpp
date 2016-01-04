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

#include "../../shared.cpp"

static int init_opengl_es(Program* program) {
  const EGLint attribs[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, // this doesn't really prohibit creating 3.0 context?
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_NONE
  };
  const EGLint context_attrib[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
  };
  EGLint format;
  EGLint numConfigs;
  EGLConfig config;
  EGLSurface surface;
  EGLContext context;
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(display, nullptr, nullptr);
  eglChooseConfig(display, attribs, &config, 1, &numConfigs);
  eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
  ANativeWindow_setBuffersGeometry(program->app->window, 0, 0, format);
  surface = eglCreateWindowSurface(display, config, program->app->window, nullptr);
  context = eglCreateContext(display, config, nullptr, context_attrib);
  if (!context) {
    LOGW("Unable to create OpenGL ES 3.0 context");
    return -1;
  }
  if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
    LOGW("Unable to eglMakeCurrent");
    return -1;
  }
  program->opengl_es.display = display;
  program->opengl_es.context = context;
  program->opengl_es.surface = surface;
  eglQuerySurface(display, surface, EGL_WIDTH, &program->opengl_es.surface_width);
  eglQuerySurface(display, surface, EGL_HEIGHT, &program->opengl_es.surface_height);

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  eglSwapBuffers(program->opengl_es.display, program->opengl_es.surface);

  return 0;
}

static void shutdown_opengl_es(Program* program) {
  auto *gl = &program->opengl_es;
  if (gl->display != EGL_NO_DISPLAY) {
    eglMakeCurrent(gl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (gl->context != EGL_NO_CONTEXT) {
      eglDestroyContext(gl->display, gl->context);
    }
    if (gl->surface != EGL_NO_SURFACE) {
      eglDestroySurface(gl->display, gl->surface);
    }
    eglTerminate(gl->display);
  }
  gl->display = EGL_NO_DISPLAY;
  gl->context = EGL_NO_CONTEXT;
  gl->surface = EGL_NO_SURFACE;
}

static bool show_soft_keyboard(android_app *app, bool show) {
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

static void handle_app_cmd(android_app* app, int32 cmd) {
  Program* program = (Program*)app->userData;
  switch (cmd) {
  case APP_CMD_INPUT_CHANGED: {
    LOGI("received APP_CMD_INPUT_CHANGED");
  } break;
  case APP_CMD_INIT_WINDOW: {
    LOGI("received APP_CMD_INIT_WINDOW");
    init_opengl_es(program);
    init_opengl_es_shaders(&program->opengl_es.shaders);
    init_font_atlas_texture(&program->font);
  } break;
  case APP_CMD_TERM_WINDOW: {
    LOGI("received APP_CMD_TERM_WINDOW");
    shutdown_opengl_es(program);
  } break;
  case APP_CMD_WINDOW_RESIZED: {
    LOGI("received APP_CMD_WINDOW_RESIZED");
  } break;
  case APP_CMD_WINDOW_REDRAW_NEEDED: {
    LOGI("received APP_CMD_WINDOW_REDRAW_NEEDED");
  } break;
  case APP_CMD_CONTENT_RECT_CHANGED: {
    LOGI("received APP_CMD_CONTENT_RECT_CHANGED");
  } break;
  case APP_CMD_GAINED_FOCUS: {
    LOGI("received APP_CMD_GAINED_FOCUS");
  } break;
  case APP_CMD_LOST_FOCUS: {
    LOGI("received APP_CMD_LOST_FOCUS");
  } break;
  case APP_CMD_CONFIG_CHANGED: {
    LOGI("received APP_CMD_CONFIG_CHANGED");
  } break;
  case APP_CMD_LOW_MEMORY: {
    LOGI("received APP_CMD_LOW_MEMORY");
  } break;
  case APP_CMD_START: {
    program->keyboard_state.shift_on = false;
    int ascent;
    stbtt_GetFontVMetrics(&program->font.info, &ascent, nullptr, nullptr);
    program->on_screen_text.pen_pos_y = ascent * program->font.scale_factor + 250;
    LOGI("received APP_CMD_START");
  } break;
  case APP_CMD_RESUME: {
    LOGI("received APP_CMD_RESUME");
  } break;
  case APP_CMD_SAVE_STATE: {
    LOGI("received APP_CMD_SAVE_STATE");
  } break;
  case APP_CMD_PAUSE: {
    LOGI("received APP_CMD_PAUSE");
  } break;
  case APP_CMD_STOP: {
    delete_str(program->on_screen_text.text);
    program->on_screen_text = {};
    LOGI("received APP_CMD_STOP");
  } break;
  case APP_CMD_DESTROY: {
    LOGI("received APP_CMD_DESTROY");
  } break;
  }
}

static char translate_keycode(int android_keycode, bool shift_on) {
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

static int32 handle_app_input(android_app* app, AInputEvent* event) {
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
      LOGI("key multiple %d", keycode);
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
    if (!init_font_data(&program->font, buf)) {
      return;
    }
  }

  { // event loop
    android_poll_source* source;
    while (ALooper_pollAll(-1, nullptr, nullptr, (void**)&source) >= 0) {
      if (source) {
        source->process(app, source);
      }
      if (app->destroyRequested) {
        shutdown_opengl_es(program);
        break;
      }
    }
  }

  return;
}
