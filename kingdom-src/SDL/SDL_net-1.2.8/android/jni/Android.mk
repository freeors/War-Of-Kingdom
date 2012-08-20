LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := SDL_net

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../SDL-1.3.0-6217/include

LOCAL_SRC_FILES := \
	SDLnet.c SDLnetselect.c SDLnetTCP.c SDLnetUDP.c
	

#LOCAL_SHARED_LIBRARIES := SDL

LOCAL_LDLIBS := -lz -lSDL

include $(BUILD_SHARED_LIBRARY)
