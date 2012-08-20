LOCAL_PATH := $(call my-dir)/../..

include $(CLEAR_VARS)

LOCAL_MODULE := intl

LOCAL_CFLAGS += -DBUILDING_LIBINTL -DHAVE_CONFIG_H -DIN_LIBINTL -DDEPENDS_ON_LIBICONV=1

LOCAL_C_INCLUDES := $(LOCAL_PATH)/gettext \
	$(LOCAL_PATH)/libiconv/include \
	$(LOCAL_PATH)/libiconv/lib \
	$(LOCAL_PATH)/libiconv/libcharset/include
	
LOCAL_SRC_FILES := \
	gettext/gettext-runtime/intl/bindtextdom.c \
	gettext/gettext-runtime/intl/dcgettext.c \
	gettext/gettext-runtime/intl/dcigettext.c \
	gettext/gettext-runtime/intl/dcngettext.c \
	gettext/gettext-runtime/intl/dgettext.c \
	gettext/gettext-runtime/intl/dngettext.c \
	gettext/gettext-runtime/intl/explodename.c \
	gettext/gettext-runtime/intl/finddomain.c \
	gettext/gettext-runtime/intl/gettext.c \
	gettext/gettext-runtime/intl/hash-string.c \
	gettext/gettext-runtime/intl/intl-compat.c \
	gettext/gettext-runtime/intl/l10nflist.c \
	gettext/gettext-runtime/intl/langprefs.c \
	gettext/gettext-runtime/intl/loadmsgcat.c \
	gettext/gettext-runtime/intl/localcharset.c \
	gettext/gettext-runtime/intl/localealias.c \
	gettext/gettext-runtime/intl/localename.c \
	gettext/gettext-runtime/intl/lock.c \
	gettext/gettext-runtime/intl/log.c \
	gettext/gettext-runtime/intl/ngettext.c \
	gettext/gettext-runtime/intl/osdep.c \
	gettext/gettext-runtime/intl/plural-exp.c \
	gettext/gettext-runtime/intl/plural.c \
	gettext/gettext-runtime/intl/printf.c \
	gettext/gettext-runtime/intl/relocatable.c \
	gettext/gettext-runtime/intl/textdomain.c \
	gettext/gettext-runtime/intl/version.c \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/libiconv/lib/*.c) \
	$(wildcard $(LOCAL_PATH)/src/addon/*.cpp) \
	$(wildcard $(LOCAL_PATH)/src/libiconv/libcharset/lib/*.c))
	
	
LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
