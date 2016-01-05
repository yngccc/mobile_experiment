LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := native-activity
LOCAL_SRC_FILES := ../../android_main.cpp
LOCAL_CPPFLAGS := -std=c++11 -fno-rtti -fno-exception
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -lz -lm -latomic
LOCAL_STATIC_LIBRARIES := android_native_app_glue
include $(BUILD_SHARED_LIBRARY)
$(call import-module,android/native_app_glue)


