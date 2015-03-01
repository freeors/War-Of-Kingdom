LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := kingdom

SDL_PATH := ../SDL/SDL2-2.0.3

LOCAL_C_INCLUDES := $(LOCAL_PATH)/external/boost \
	$(LOCAL_PATH)/external/bzip2 \
	$(LOCAL_PATH)/external/zlib \
	$(LOCAL_PATH)/../gettext/gettext-framework/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL-2.0.x/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_image/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_mixer/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_net/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_ttf/include \
	$(LOCAL_PATH)/librose \
	$(LOCAL_PATH)/src
	
LOCAL_CFLAGS :=	-D_KINGDOM_EXE

LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/src/*.c) \
	$(wildcard $(LOCAL_PATH)/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/ai/*.c) \
	$(wildcard $(LOCAL_PATH)/src/ai/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/ai/default/*.c) \
	$(wildcard $(LOCAL_PATH)/src/ai/default/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/editor/*.c) \
	$(wildcard $(LOCAL_PATH)/src/editor/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/lua/*.c) \
	$(wildcard $(LOCAL_PATH)/src/lua/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/pathfind/*.c) \
	$(wildcard $(LOCAL_PATH)/src/pathfind/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/scripting/*.c) \
	$(wildcard $(LOCAL_PATH)/src/scripting/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/lobby/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/lobby/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/event/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/event/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/iterator/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/iterator/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/placer/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/placer/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/widget_Definition/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/widget_Definition/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/window_Builder/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/auxiliary/window_Builder/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/dialogs/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/dialogs/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/lib/types/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/lib/types/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/gui/widgets/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/gui/widgets/*.cpp) \
	$(wildcard $(LOCAL_PATH)/librose/serialization/*.c) \
	$(wildcard $(LOCAL_PATH)/librose/serialization/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/iostreams/src/*.c) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/iostreams/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/regex/src/*.c) \
	$(wildcard $(LOCAL_PATH)/external/boost/libs/regex/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/bzip2/*.c) \
	$(wildcard $(LOCAL_PATH)/external/bzip2/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/zlib/*.c) \
	$(wildcard $(LOCAL_PATH)/external/zlib/*.cpp)) 
	
#LOCAL_SHARED_LIBRARIES := intl SDL2 SDL2_image SDL2_mixer SDL2_net SDL2_ttf

LOCAL_STATIC_LIBRARIES := libgnustl_shared.so

#LOCAL_LDLIBS := -llog
LOCAL_LDLIBS := -llog -lintl -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_net -lSDL2_ttf

include $(BUILD_SHARED_LIBRARY)
