#!/bin/sh


POSTBUILD=/Users/ancientcc/ddksample/postbuild
SDL_sdl=$POSTBUILD/../SDL/SDL2-2.0.3/Xcode-iOS/SDL/build/Release-iphoneos
SDL_image=$POSTBUILD/../SDL/SDL2_image-2.0.0/Xcode-iOS/build/Release-iphoneos
SDL_mixer=$POSTBUILD/../SDL/SDL2_mixer-2.0.0/Xcode-iOS/build/Release-iphoneos
SDL_net=$POSTBUILD/../SDL/SDL2_net-2.0.0/Xcode-iOS/build/Release-iphoneos
SDL_ttf=$POSTBUILD/../SDL/SDL2_ttf-2.0.12/Xcode-iOS/build/Release-iphoneos

GETTEXT=$POSTBUILD/../gettext/Xcode-iPhoneOS/build/Release-iphoneos

KINGDOM=$POSTBUILD/../kingdom/projectfiles/Xcode-iPhoneOS/lib/-iphoneos

cp -p $GETTEXT/libintl.a $KINGDOM/.
 
cp -p $SDL_sdl/libSDL2.a $KINGDOM/.
 
cp -p $SDL_image/libSDL2_image.a $KINGDOM/.

cp -p $SDL_mixer/libSDL2_mixer.a $KINGDOM/.

cp -p $SDL_net/libSDL2_net.a $KINGDOM/.

cp -p $SDL_ttf/libSDL2_ttf.a $KINGDOM/.