#!/bin/sh

. ~/.bash_profile

cp $GETTEXT/libs/armeabi/libintl.so $KINGDOM/libs/armeabi/.

cp $FREETYPE/libs/armeabi/libfreetype.so $KINGDOM/libs/armeabi/.
cp $PNG/libs/armeabi/libpng.so $KINGDOM/libs/armeabi/.
cp $JPEG/libjpeg.so $KINGDOM/libs/armeabi/.
cp $VORBIS/libs/armeabi/libvorbis.so $KINGDOM/libs/armeabi/.

cp $SDL_sdl/libs/armeabi/libSDL.so $KINGDOM/libs/armeabi/.
cp $SDL_net/libs/armeabi/libSDL_net.so $KINGDOM/libs/armeabi/.
cp $SDL_image/libs/armeabi/libSDL_image.so $KINGDOM/libs/armeabi/.
cp $SDL_mixer/libs/armeabi/libSDL_mixer.so $KINGDOM/libs/armeabi/.
cp $SDL_ttf/libs/armeabi/libSDL_ttf.so $KINGDOM/libs/armeabi/.