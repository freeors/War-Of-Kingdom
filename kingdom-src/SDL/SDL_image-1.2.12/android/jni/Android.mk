LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := SDL_image

LOCAL_CFLAGS := -I$(LOCAL_PATH)/../jpeg-8d -I$(LOCAL_PATH)/../lpng157 -I$(LOCAL_PATH)/../SDL-1.3.0-6217/include \
	-DLOAD_JPG -DLOAD_PNG -DLOAD_BMP -DLOAD_GIF -DLOAD_LBM \
	-DLOAD_PCX -DLOAD_PNM -DLOAD_TGA -DLOAD_XCF -DLOAD_XPM \
	-DLOAD_XV

LOCAL_SRC_FILES := $(notdir $(filter-out %/showimage.c, $(wildcard $(LOCAL_PATH)/*.c)))

#LOCAL_SHARED_LIBRARIES := SDL

#LOCAL_STATIC_LIBRARIES := png jpeg

LOCAL_LDLIBS := -lz -lSDL -ljpeg -lpng

include $(BUILD_SHARED_LIBRARY)
