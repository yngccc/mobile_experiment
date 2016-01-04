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

static Program *program = CALLOC(Program, 1);

@interface KeyboardView : UIView <UIKeyInput>
@end
@implementation KeyboardView
- (void)insertText:(NSString *)text {
  int c_str_len = (int)text.length;
  const char *c_str = text.UTF8String;
  for (int i = 0; i < c_str_len; ++i) {
    add_char_to_on_screen_text_verts_buf(program, c_str[i]);
  }
  render_on_screen_text(program);
}
- (void)deleteBackward {
}
- (BOOL)hasText {
  return YES;
}
- (BOOL)canBecomeFirstResponder {
  return YES;
}
@end

@interface OpenGLView : GLKView
- (void)drawRect:(CGRect)rect;
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event;
@end
@implementation OpenGLView
- (void)drawRect:(CGRect)rect {
  program->opengl_es.surface_width = (int)self.drawableWidth;
  program->opengl_es.surface_height = (int)self.drawableHeight;
  render_font_atlas(program);
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
  static bool show = false;
  show = !show;
  KeyboardView *keyboard_view = self.subviews[0];
  if (show) {
    [keyboard_view becomeFirstResponder];
  } else {
    [keyboard_view resignFirstResponder];
  }
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
}
@end

@interface OpenGLViewController : UIViewController
- (void)loadView;
- (void)viewDidLoad;
@end
@implementation OpenGLViewController
- (void)loadView {
  EAGLContext *context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  [EAGLContext setCurrentContext:context];
  OpenGLView *opengl_view = [[OpenGLView alloc] initWithFrame:[[UIScreen mainScreen] bounds] context:context];
  opengl_view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
  opengl_view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
  opengl_view.drawableStencilFormat = GLKViewDrawableStencilFormat8;
  opengl_view.drawableMultisample = GLKViewDrawableMultisample4X;
  KeyboardView *keyboard_view = [[KeyboardView alloc] init];
  [opengl_view addSubview:keyboard_view];
  self.view = opengl_view;
}
- (void)viewDidLoad {
  NSString *font_path = [[NSBundle mainBundle] pathForResource:@"OpenSans-Regular" ofType:@"ttf" inDirectory:@"assets/fonts/open-sans"];
  assert(font_path != nil);
  NSData *font_ns_data = [NSData dataWithContentsOfFile:font_path];
  NSInteger font_ns_data_len = [font_ns_data length];
  byte *font_data = MALLOC(byte, (int)font_ns_data_len);
  memcpy(font_data, [font_ns_data bytes], font_ns_data_len);
  init_font_data(&program->font, font_data);
  init_opengl_es_shaders(&program->opengl_es.shaders);
  init_font_atlas_texture(&program->font);
}
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  CGRect screenBounds = [[UIScreen mainScreen] bounds];
  UIWindow *window = [[UIWindow alloc] initWithFrame:screenBounds];
  OpenGLViewController *viewController = [[OpenGLViewController alloc] init];
  [window setRootViewController:viewController];
  [window makeKeyAndVisible];
  [self setWindow:window];
  LOGI("didFinishLaunchingWithOptions");
  return YES;
}
- (void)applicationDidBecomeActive:(UIApplication *)application {
  render_font_atlas(program);
  str_set_c(&program->on_screen_text.text, "");
  program->on_screen_text.gl_verts_buf_size_in_use = 0;
  int ascent;
  stbtt_GetFontVMetrics(&program->font.info, &ascent, nullptr, nullptr);
  program->on_screen_text.pen_pos_x = 0;
  program->on_screen_text.pen_pos_y = ascent * program->font.scale_factor + 250;
  LOGI("applicationDidBecomeActive");
}
- (void)applicationWillResignActive:(UIApplication *)application {
  LOGI("applicationWillResignActive");
}
- (void)applicationDidEnterBackground:(UIApplication *)application {
  LOGI("applicationDidEnterBackground");
}
- (void)applicationWillEnterForeground:(UIApplication *)application {
  LOGI("applicationWillEnterForeground");
}
- (void)applicationWillTerminate:(UIApplication *)application {
  LOGI("applicationWillTerminate");
}
@end

int main(int argc, char * argv[]) {
  @autoreleasepool {
      return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
