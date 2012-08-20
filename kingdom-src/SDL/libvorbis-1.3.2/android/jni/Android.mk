LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := vorbis

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include $(LOCAL_PATH)/../libogg-1.3.0/include

LOCAL_SRC_FILES := ../libogg-1.3.0/src/bitwise.c \
	../libogg-1.3.0/src/framing.c \
	lib/analysis.c \
	lib/bitrate.c \
	lib/block.c \
	lib/codebook.c \
	lib/envelope.c \
	lib/floor0.c \
	lib/floor1.c \
	lib/info.c \
	lib/lookup.c \
	lib/lpc.c \
	lib/lsp.c \
	lib/mapping0.c \
	lib/mdct.c \
	lib/psy.c \
	lib/registry.c \
	lib/res0.c \
	lib/sharedbook.c \
	lib/smallft.c \
	lib/synthesis.c \
	lib/vorbisfile.c \
	lib/window.c
	
LOCAL_LDLIBS := -lz

include $(BUILD_SHARED_LIBRARY)
