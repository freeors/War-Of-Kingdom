LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := SDL2_net

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../SDL2-2.0.3/include

LOCAL_SRC_FILES := \
	SDLnet.c SDLnetselect.c SDLnetTCP.c SDLnetUDP.c
	

#LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lSDL2

include $(BUILD_SHARED_LIBRARY)
