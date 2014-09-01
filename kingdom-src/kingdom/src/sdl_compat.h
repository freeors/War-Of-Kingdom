/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

 /**
 *  \defgroup Compatibility SDL 1.2 Compatibility API
 */
/*@{*/

/**
 *  \file SDL_compat.h
 *
 *  This file contains functions for backwards compatibility with SDL 1.2.
 */

/**
 *  \def SDL_NO_COMPAT
 *
 *  #define SDL_NO_COMPAT to prevent SDL_compat.h from being included.
 *  SDL_NO_COMPAT is intended to make it easier to covert SDL 1.2 code to
 *  SDL 1.3/2.0.
 */

 /*@}*/

#ifdef SDL_NO_COMPAT
#define _SDL_compat_h
#endif

#ifndef _SDL_compat_h
#define _SDL_compat_h

// #include "SDL_video.h"
// #include "SDL_version.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/**
 *  \addtogroup Compatibility
 */
/*@{*/

/**
 *  \name Surface flags
 */
/*@{*/
#define SDL_SRCALPHA        0x00010000
#define SDL_SRCCOLORKEY     0x00020000
#define SDL_ANYFORMAT       0x00100000
#define SDL_HWPALETTE       0x00200000
#define SDL_DOUBLEBUF       0x00400000
#define SDL_FULLSCREEN      0x00800000
#define SDL_RESIZABLE       0x01000000
#define SDL_NOFRAME         0x02000000
#define SDL_OPENGL          0x04000000
#define SDL_HWSURFACE       0x08000001  /**< \note Not used */
#define SDL_ASYNCBLIT       0x08000000  /**< \note Not used */
#define SDL_RLEACCELOK      0x08000000  /**< \note Not used */
#define SDL_HWACCEL         0x08000000  /**< \note Not used */
/*@}*//*Surface flags*/

#define SDL_APPMOUSEFOCUS	0x01
#define SDL_APPINPUTFOCUS	0x02
#define SDL_APPACTIVE		0x04

#define SDL_MOUSEMOTIONMASK SDL_MOUSEMOTION, SDL_MOUSEMOTION

struct SDL_SysWMinfo;

/**
 *  \name Obsolete or renamed key codes
 */
/*@{*/

#define SDL_keysym		SDL_Keysym
#define SDL_KeySym		SDL_Keysym
#define SDL_scancode	SDL_Scancode
#define SDL_ScanCode	SDL_Scancode
#define SDLKey          SDL_Keycode
#define SDLMod          SDL_Keymod

/** 
 *  \name Not in USB
 *
 *  These keys don't appear in the USB specification (or at least not under 
 *  those names). I'm unsure if the following assignments make sense or if these
 *  codes should be defined as actual additional SDLK_ constants.
 */
/*@{*/
#define SDLK_LSUPER SDLK_LMETA
#define SDLK_RSUPER SDLK_RMETA
/*@}*//*Not in USB*/

/*@}*//*Obsolete or renamed key codes*/

extern DECLSPEC SDL_Rect **SDLCALL SDL_ListModes(const SDL_PixelFormat *
                                                 format, Uint32 flags);
extern DECLSPEC int SDLCALL SDL_SetAlpha(SDL_Surface * surface,
                                         Uint32 flag, Uint8 alpha);
extern DECLSPEC SDL_Surface *SDLCALL SDL_DisplayFormatAlpha(SDL_Surface* screen_surf, SDL_Surface *
                                                            surface);
extern DECLSPEC int SDLCALL SDL_GetWMInfo(struct SDL_SysWMinfo *info);
extern DECLSPEC Uint8 SDLCALL SDL_GetAppState(SDL_Window* window);
extern DECLSPEC int SDLCALL SDL_EnableUNICODE(int enable);

#define SDL_KillThread(X)

/* The timeslice and timer resolution are no longer relevant */
#define SDL_TIMESLICE		10
#define TIMER_RESOLUTION	10

typedef Uint32 (SDLCALL * SDL_OldTimerCallback) (Uint32 interval);
extern DECLSPEC int SDLCALL SDL_SetTimer(Uint32 interval, SDL_OldTimerCallback callback);

/*@}*//*Compatibility*/

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif
#include "close_code.h"

#endif /* _SDL_compat_h */

/* vi: set ts=4 sw=4 expandtab: */
