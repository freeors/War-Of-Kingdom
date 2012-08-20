#!/bin/sh


POSTBUILD=/Users/ancientcc/ddksample/postbuild
SDL_sdl=$POSTBUILD/../SDL/SDL-1.3.0-6217/Xcode-iPhoneOS/SDL/build/Debug-iphonesimulator
SDL_image=$POSTBUILD/../SDL/SDL_image-1.2.12/Xcode-iOS/build/Debug-iphonesimulator
SDL_mixer=$POSTBUILD/../SDL/SDL_mixer-1.2.12/Xcode-iOS/build/Debug-iphonesimulator
SDL_net=$POSTBUILD/../SDL/SDL_net-1.2.8/Xcode-iOS/build/Debug-iphonesimulator
SDL_ttf=$POSTBUILD/../SDL/SDL_ttf-2.0.11/Xcode-iPhoneOS/build/Debug-iphonesimulator

GETTEXT=$POSTBUILD/../gettext/Xcode-iPhoneOS/build/Debug-iphonesimulator

KINGDOM=$POSTBUILD/../kingdom/projectfiles/Xcode-iPhoneOS/lib/-iphonesimulator

cp -p $GETTEXT/libintl.a $KINGDOM/.
 
cp -p $SDL_sdl/libSDL.a $KINGDOM/.
 
cp -p $SDL_image/libSDL_image.a $KINGDOM/.

cp -p $SDL_mixer/libSDL_mixer.a $KINGDOM/.

cp -p $SDL_net/libSDL_net.a $KINGDOM/.

cp -p $SDL_ttf/libSDL_ttf.a $KINGDOM/.