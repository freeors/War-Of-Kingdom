LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := SDL_ttf

LOCAL_CFLAGS := -I$(LOCAL_PATH)/../SDL-1.3.0-6217/include -I$(LOCAL_PATH)/../freetype-2.3.9/include

LOCAL_SRC_FILES := SDL_ttf.c

#LOCAL_SHARED_LIBRARIES := SDL

#LOCAL_STATIC_LIBRARIES := freetype

LOCAL_LDLIBS := -lz -lSDL -lfreetype

include $(BUILD_SHARED_LIBRARY)
