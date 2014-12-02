/* $Id: video.cpp 46847 2010-10-01 01:43:16Z alink $ */
/*
   Copyright (C) 2003 - 2010 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file
 *  Video-testprogram, standalone
 */

#include "global.hpp"

#include "font.hpp"
#include "image.hpp"
#include "log.hpp"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "sdl_utils.hpp"
#include "video.hpp"
#include "display.hpp"

#include <boost/foreach.hpp>
#include <vector>
#include <map>
#include <algorithm>

static lg::log_domain log_display("display");
#define LOG_DP LOG_STREAM(info, log_display)
#define ERR_DP LOG_STREAM(err, log_display)

namespace {
	bool fullScreen = false;
	int disallow_resize = 0;
}
void resize_monitor::process(events::pump_info &info) 
{
	if (info.resize_dimensions.first >= preferences::min_allowed_width() && info.resize_dimensions.second >= preferences::min_allowed_height()
		&& disallow_resize == 0) {
		preferences::set_resolution(info.resize_dimensions);
	}
}

resize_lock::resize_lock()
{
	++disallow_resize;
}

resize_lock::~resize_lock()
{
	--disallow_resize;
}

static unsigned int get_flags(unsigned int flags)
{
#if defined(__APPLE__) && TARGET_OS_IPHONE
	flags |= SDL_WINDOW_BORDERLESS;
#else
	if (!(flags & SDL_WINDOW_FULLSCREEN))
		flags |= SDL_WINDOW_RESIZABLE;
#endif

	return flags;
}

namespace {
struct event {
	int x, y, w, h;
	bool in;
	event(const SDL_Rect& rect, bool i) : x(i ? rect.x : rect.x + rect.w), y(rect.y), w(rect.w), h(rect.h), in(i) { }
};
bool operator<(const event& a, const event& b) {
	if (a.x != b.x) return a.x < b.x;
	if (a.in != b.in) return a.in;
	if (a.y != b.y) return a.y < b.y;
	if (a.h != b.h) return a.h < b.h;
	if (a.w != b.w) return a.w < b.w;
	return false;
}
bool operator==(const event& a, const event& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h && a.in == b.in;
}

struct segment {
	int x, count;
	segment() : x(0), count(0) { }
	segment(int x, int count) : x(x), count(count) { }
};

}

namespace {
Uint32 current_format = SDL_PIXELFORMAT_UNKNOWN;
SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
surface frameBuffer = NULL;
SDL_Texture* frameTexture = NULL;

}

surface display_format_alpha(surface surf)
{
	if (frameBuffer) {
		return SDL_DisplayFormatAlpha(frameBuffer, surf);
	} else {
		return NULL;
	}
}

surface get_video_surface()
{
	return frameBuffer;
}

SDL_Rect screen_area()
{
	return create_rect(0, 0, frameBuffer->w, frameBuffer->h);
}

CVideo::CVideo() : mode_changed_(false), bpp_(0), help_string_(0), updatesLocked_(0)
{
	initSDL();
}

void CVideo::initSDL()
{
	const int res = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);

	if (res < 0) {
		throw CVideo::error();
	}
}

CVideo::~CVideo()
{
	if (window) {
		SDL_DestroyWindow(window);
	}
	if (frameTexture) {
		SDL_DestroyTexture(frameTexture);
	}
	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}
	SDL_Quit();
}

void CVideo::blit_surface(int x, int y, surface surf, SDL_Rect* srcrect, SDL_Rect* clip_rect)
{
	surface target(getSurface());
	SDL_Rect dst = create_rect(x, y, 0, 0);

	const clip_rect_setter clip_setter(target, clip_rect, clip_rect != NULL);
	sdl_blit(surf,srcrect,target,&dst);
}

int CVideo::modePossible(int w, int h, int bits_per_pixel, int flags, bool current_screen_optimal )
{
	SDL_Rect screen_rect = bound();

	if (w < preferences::min_allowed_width()) {
		w = preferences::min_allowed_width();
	} else if (w > screen_rect.w) {
		w = screen_rect.w;
	}

	if (h < preferences::min_allowed_height()) {
		h = preferences::min_allowed_height();

	} else if (h > screen_rect.h) {
		h = screen_rect.h;
	}

	return 32;
}

int CVideo::setMode(int x, int y, int bits_per_pixel, int flags)
{
	bool reset_zoom = frameBuffer? false: true;

	mode_changed_ = true;

	flags = get_flags(flags);

	if (current_format == SDL_PIXELFORMAT_UNKNOWN) {
		current_format = SDL_PIXELFORMAT_ARGB8888;
	}

	fullScreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	if (window) {
		SDL_DestroyWindow(window);
	}
	if (frameTexture) {
		SDL_DestroyTexture(frameTexture);
	}
	if (renderer) {
		SDL_DestroyRenderer(renderer);
	}
	
	window = SDL_CreateWindow("War of Kingdom",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          x, y,
                          flags);

	int ret_w, ret_h;
	SDL_GetWindowSize(window, &ret_w, &ret_h);

	renderer = SDL_CreateRenderer(window, -1, 0);
	frameBuffer = SDL_CreateRGBSurface(0, x, y, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_SetSurfaceBlendMode(frameBuffer, SDL_BLENDMODE_NONE);
	frameTexture = SDL_CreateTexture(renderer, current_format, SDL_TEXTUREACCESS_STREAMING, x, y);

	// frameBuffer's refcount should be 1. If not, check SDL_SetVideoMode!
	// 1 is holded by frameBuffer.
	if (frameBuffer.get()->refcount != 1) {
		return 0;
	}
	
	if (frameBuffer != NULL) {
		image::set_pixel_format(frameBuffer->format);
		if (frameBuffer->w < 800 || frameBuffer->h < 600) {
			game_config::tiny_gui = true;
		} else {
			game_config::tiny_gui = false;
		}
		if (reset_zoom) {
			set_zoom_to_default(preferences::zoom());
		}
		return bits_per_pixel;
	} else	{
		return 0;
	}
}

bool CVideo::modeChanged()
{
	bool ret = mode_changed_;
	mode_changed_ = false;
	return ret;
}

int CVideo::getx() const
{
	return frameBuffer->w;
}

int CVideo::gety() const
{
	return frameBuffer->h;
}

SDL_Rect CVideo::bound() const
{
	SDL_Rect rc;
	int display = window? SDL_GetWindowDisplayIndex(window): 0;
	SDL_GetDisplayBounds(display, &rc);
	return rc;
}

Uint32 CVideo::getformat() const
{
	return current_format;
}

void CVideo::flip()
{
	SDL_UpdateTexture(frameTexture, NULL, frameBuffer->pixels, frameBuffer->w * sizeof (Uint32));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, frameTexture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

void CVideo::lock_updates(bool value)
{
	if(value == true)
		++updatesLocked_;
	else
		--updatesLocked_;
}

bool CVideo::update_locked() const
{
	return updatesLocked_ > 0;
}

surface& CVideo::getSurface()
{
	return frameBuffer;
}

SDL_Window* CVideo::getWindow()
{
	return window;
}

bool CVideo::isFullScreen() const { return fullScreen; }

void CVideo::setBpp( int bpp )
{
	bpp_ = bpp;
}

int CVideo::getBpp()
{
	return bpp_;
}

int CVideo::set_help_string(const std::string& str)
{
	font::remove_floating_label(help_string_);

	const SDL_Color color = { 0, 0, 0, 0xbb };

	int size = font::SIZE_LARGE;

	while(size > 0) {
		if(font::line_width(str, size) > getx()) {
			size--;
		} else {
			break;
		}
	}

	const int border = 5;

	font::floating_label flabel(str);
	flabel.set_font_size(size);
	flabel.set_position(getx()/2, gety());
	flabel.set_bg_color(color);
	flabel.set_border_size(border);

	help_string_ = font::add_floating_label(flabel);

	const SDL_Rect& rect = font::get_floating_label_rect(help_string_);
	font::move_floating_label(help_string_,0.0,-double(rect.h));
	return help_string_;
}

void CVideo::clear_help_string(int handle)
{
	if(handle == help_string_) {
		font::remove_floating_label(handle);
		help_string_ = 0;
	}
}

void CVideo::clear_all_help_strings()
{
	clear_help_string(help_string_);
}
