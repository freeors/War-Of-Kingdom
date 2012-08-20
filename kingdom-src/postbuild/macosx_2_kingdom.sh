#!/bin/sh


POSTBUILD=/Users/ancientcc/ddksample/postbuild
SDL_sdl=$POSTBUILD/../SDL/SDL-1.3.0-6217/Xcode/SDL/build/Release
SDL_image=$POSTBUILD/../SDL/SDL_image-1.2.12/Xcode/build/Release
SDL_mixer=$POSTBUILD/../SDL/SDL_mixer-1.2.12/Xcode/build/Release
SDL_net=$POSTBUILD/../SDL/SDL_net-1.2.8/Xcode/build/Release
SDL_ttf=$POSTBUILD/../SDL/SDL_ttf-2.0.11/Xcode/build/Release
SMPEG=$POSTBUILD/../SDL/smpeg/Xcode/build/Release

GETTEXT=$POSTBUILD/../gettext/Xcode/build/Release

KINGDOM=$POSTBUILD/../kingdom/projectfiles/Xcode/lib

rm -rf $KINGDOM/libintl.framework 
cp -a $GETTEXT/libintl.framework $KINGDOM/.

rm -rf $KINGDOM/SDL.framework 
cp -a $SDL_sdl/SDL.framework $KINGDOM/.

rm -rf $KINGDOM/SDL_image.framework 
cp -a $SDL_image/SDL_image.framework $KINGDOM/.

rm -rf $KINGDOM/SDL_mixer.framework 
cp -a $SDL_mixer/SDL_mixer.framework $KINGDOM/.

rm -rf $KINGDOM/SDL_net.framework 
cp -a $SDL_net/SDL_net.framework $KINGDOM/.

rm -rf $KINGDOM/SDL_ttf.framework 
cp -a $SDL_ttf/SDL_ttf.framework $KINGDOM/.

rm -rf $KINGDOM/smpeg.framework 
cp -a $SMPEG/smpeg.framework $KINGDOM/.