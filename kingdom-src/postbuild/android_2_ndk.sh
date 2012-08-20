#!/bin/sh

. ~/.bash_profile

cp $GETTEXT/libs/armeabi/libintl.so $NDK/platforms/android-9/arch-arm/usr/lib/.

cp $FREETYPE/libs/armeabi/libfreetype.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $PNG/libs/armeabi/libpng.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $JPEG/libjpeg.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $VORBIS/libs/armeabi/libvorbis.so $NDK/platforms/android-9/arch-arm/usr/lib/.

cp $SDL_sdl/libs/armeabi/libSDL.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $SDL_net/libs/armeabi/libSDL_net.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $SDL_image/libs/armeabi/libSDL_image.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $SDL_mixer/libs/armeabi/libSDL_mixer.so $NDK/platforms/android-9/arch-arm/usr/lib/.
cp $SDL_ttf/libs/armeabi/libSDL_ttf.so $NDK/platforms/android-9/arch-arm/usr/lib/.