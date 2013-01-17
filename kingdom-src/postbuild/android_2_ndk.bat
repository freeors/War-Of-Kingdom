set DST=%NDK%\platforms\android-9\arch-arm\usr\lib\.

copy %GETTEXT%\libs\armeabi\libintl.so %DST%

copy %FREETYPE%\libs\armeabi\libfreetype.so %DST%
copy %PNG%\libs\armeabi\libpng.so %DST%
REM copy %JPEG%\libjpeg.so %DST%
copy %VORBIS%\libs\armeabi\libvorbis.so %DST%

copy %SDL_sdl%\libs\armeabi\libSDL.so %DST%
copy %SDL_net%\libs\armeabi\libSDL_net.so %DST%
copy %SDL_image%\libs\armeabi\libSDL_image.so %DST%
copy %SDL_mixer%\libs\armeabi\libSDL_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi\libSDL_ttf.so %DST%