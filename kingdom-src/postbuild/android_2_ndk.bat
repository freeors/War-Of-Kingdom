set DST=%NDK%\platforms\android-9\arch-arm\usr\lib\.

copy %GETTEXT%\libs\armeabi\libintl.so %DST%

copy %SDL_sdl%\libs\armeabi\libSDL2.so %DST%
copy %SDL_net%\libs\armeabi\libSDL2_net.so %DST%
copy %SDL_image%\libs\armeabi\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi\libSDL2_ttf.so %DST%