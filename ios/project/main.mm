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
  // Do something with the typed character
  NSLog(@"%@", text);
}
- (void)deleteBackward {
  // Handle the delete key
  NSLog(@"delete backward");
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
  LOGI("draw rect");
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
  if (!init_font_data(&program->font, font_data)) {
    exit(1);
  }
  init_opengl_es_shaders(&program->opengl_es.shaders);
  init_font_atlas_texture(&program->font);
  LOGI("view did load");
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
  // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
  LOGI("applicationDidBecomeActive");
}

- (void)applicationWillResignActive:(UIApplication *)application {
  // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
  // Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
  LOGI("applicationWillResignActive");
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
  // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
  // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
  LOGI("applicationDidEnterBackground");
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
  // Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
  LOGI("applicationWillEnterForeground");
}


- (void)applicationWillTerminate:(UIApplication *)application {
  // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
  LOGI("applicationWillTerminate");
}
@end

int main(int argc, char * argv[]) {
  @autoreleasepool {
      return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
