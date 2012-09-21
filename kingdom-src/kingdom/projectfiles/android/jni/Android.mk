LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := kingdom

SDL_PATH := ../SDL/SDL-1.3.0-6217

LOCAL_C_INCLUDES := $(LOCAL_PATH)/external/boost_1_46 \
	$(LOCAL_PATH)/external/zlib123 \
	$(LOCAL_PATH)/../gettext/gettext-framework/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL-1.3.x/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_image/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_mixer/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_net/include \
	$(LOCAL_PATH)/../SDL/SDL-dev-framework/SDL_ttf/include \
	$(LOCAL_PATH)/src
	
LOCAL_CFLAGS :=	-D_KINGDOM_EXE

LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.cpp \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/src/*.c) \
	$(wildcard $(LOCAL_PATH)/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/ai/*.c) \
	$(wildcard $(LOCAL_PATH)/src/ai/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/ai/default/*.c) \
	$(wildcard $(LOCAL_PATH)/src/ai/default/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/editor/*.c) \
	$(wildcard $(LOCAL_PATH)/src/editor/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/event/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/event/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/iterator/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/iterator/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/placer/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/placer/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/widget_Definition/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/widget_Definition/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/window_Builder/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/auxiliary/window_Builder/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/lobby/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/dialogs/lobby/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/lib/types/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/lib/types/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/gui/widgets/*.c) \
	$(wildcard $(LOCAL_PATH)/src/gui/widgets/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/pathfind/*.c) \
	$(wildcard $(LOCAL_PATH)/src/pathfind/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/scripting/*.c) \
	$(wildcard $(LOCAL_PATH)/src/scripting/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/storyscreen/*.c) \
	$(wildcard $(LOCAL_PATH)/src/storyscreen/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/widgets/*.c) \
	$(wildcard $(LOCAL_PATH)/src/widgets/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/serialization/*.c) \
	$(wildcard $(LOCAL_PATH)/src/serialization/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/lua/*.c) \
	$(wildcard $(LOCAL_PATH)/src/lua/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/boost_1_46/libs/iostreams/src/*.c) \
	$(wildcard $(LOCAL_PATH)/external/boost_1_46/libs/iostreams/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/boost_1_46/libs/regex/src/*.c) \
	$(wildcard $(LOCAL_PATH)/external/boost_1_46/libs/regex/src/*.cpp) \
	$(wildcard $(LOCAL_PATH)/external/zlib123/*.c) \
	$(wildcard $(LOCAL_PATH)/external/zlib123/*.cpp)) 
	
#LOCAL_SHARED_LIBRARIES := intl SDL SDL_image SDL_mixer SDL_net SDL_ttf

LOCAL_STATIC_LIBRARIES := libgnustl_shared.so

#LOCAL_LDLIBS := -llog
LOCAL_LDLIBS := -llog -lintl -lSDL -lSDL_image -lSDL_mixer -lSDL_net -lSDL_ttf

include $(BUILD_SHARED_LIBRARY)
