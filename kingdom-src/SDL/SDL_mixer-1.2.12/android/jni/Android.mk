LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := SDL_mixer

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/.. \
	$(LOCAL_PATH)/../SDL-1.3.0-6217/include \
	$(LOCAL_PATH)/../libogg-1.3.0/include \
	$(LOCAL_PATH)/../libvorbis-1.3.2/include

#LOCAL_CFLAGS := -DWAV_MUSIC -DOGG_MUSIC -DOGG_USE_TREMOR -DMOD_MUSIC
LOCAL_CFLAGS := -DWAV_MUSIC -DOGG_MUSIC

LOCAL_SRC_FILES := $(notdir $(filter-out %/playmus.c %/playwave.c, $(wildcard $(LOCAL_PATH)/*.c)))

#LOCAL_SHARED_LIBRARIES := SDL mikmod
#LOCAL_STATIC_LIBRARIES := tremor

LOCAL_LDLIBS := -lSDL -lvorbis

include $(BUILD_SHARED_LIBRARY)
