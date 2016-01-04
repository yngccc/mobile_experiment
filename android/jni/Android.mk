LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := native-activity
LOCAL_SRC_FILES := main.cpp
LOCAL_CPPFLAGS := -std=c++11
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -lz -lm
LOCAL_STATIC_LIBRARIES := android_native_app_glue
include $(BUILD_SHARED_LIBRARY)
$(call import-module,android/native_app_glue)


