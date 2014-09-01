# The ARMv7 is significanly faster due to the use of the hardware FPU
#APP_ABI := armeabi armeabi-v7a
APP_ABI := armeabi
APP_PLATFORM := android-9

APP_STL := gnustl_shared
APP_GNUSTL_FORCE_CPP_FEATURES := exceptions rtti
APP_CFLAGS += -Wno-error=format-security