/* $Id: preferences_display.cpp 47608 2010-11-21 01:56:29Z shadowmaster $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

/**
 *  @file
 *  Manage display-related preferences, e.g. screen-size, etc.
 */

#include "global.hpp"
#include "preferences_display.hpp"

#include "display.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/simple_item_selector.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "wml_separators.hpp"

#include <boost/foreach.hpp>

bool require_change_resolution = false;

namespace preferences {

bool detect_video_settings(CVideo& video, std::pair<int,int>& resolution, int& bpp, int& video_flags)
{
	video_flags = fullscreen() ? SDL_WINDOW_FULLSCREEN: 0;
	resolution = preferences::resolution();

	bpp = 32;
	return true;
}

void set_fullscreen(display& disp, const bool ison)
{
	_set_fullscreen(ison);

	CVideo& video = disp.video();
	const std::pair<int,int>& res = resolution();
	if (video.isFullScreen() != ison) {
		const int flags = ison ? SDL_WINDOW_FULLSCREEN: 0;
		int bpp = video.modePossible(res.first, res.second, 32, flags);
		VALIDATE(bpp > 0, "bpp must be large than 0!");

		video.setMode(res.first, res.second,bpp, flags);
		require_change_resolution = true;
	}
}

void set_scroll_to_action(bool ison)
{
	_set_scroll_to_action(ison);
}

bool set_resolution(display& disp, const unsigned width, const unsigned height)
{
	// - Ayin: disabled the following code. Why would one want to enforce that?
	// Some 16:9, or laptop screens, may have resolutions which do not
	// comply to this rule (see bug 10630).
	// I'm commenting this until it proves absolutely necessary.
	//
	// Make sure resolutions are always divisible by 4
	//res.first &= ~3;
	//res.second &= ~3;

	SDL_Rect rect;
	SDL_GetClipRect(disp.video().getSurface(), &rect);
	if (rect.w == width && rect.h == height) {
		return true;
	}
	if (width < 480 || height < 320) {
		return false;
	}

	CVideo& video = disp.video();
	const int flags = fullscreen() ? SDL_WINDOW_FULLSCREEN : 0;
	int bpp = video.modePossible(width, height, 32, flags);
	VALIDATE(bpp > 0, "bpp must be large than 0!");

	video.setMode(width, height, bpp, flags);
	require_change_resolution = true;

	const std::string postfix = fullscreen() ? "resolution" : "windowsize";
	preferences::set('x' + postfix, lexical_cast<std::string>(width));
	preferences::set('y' + postfix, lexical_cast<std::string>(height));
	
	return true;
}

void set_turbo(display& disp, bool ison)
{
	_set_turbo(ison);
	disp.set_turbo(ison);
}

void set_turbo_speed(display& disp, double speed)
{
	save_turbo_speed(speed);
	disp.set_turbo_speed(speed);
}

void set_grid(display& disp, bool ison)
{
	_set_grid(ison);
	disp.set_grid(ison);
}

void set_default_move(bool ison) 
{
	_set_default_move(ison);
}

bool compare_resolutions(const std::pair<int,int>& lhs, const std::pair<int,int>& rhs)
{
	return lhs.first*lhs.second < rhs.first*rhs.second;
}

bool show_video_mode_dialog(display& disp)
{
	CVideo& video = disp.video();

	SDL_PixelFormat format = *video.getSurface()->format;
	format.BitsPerPixel = video.getBpp();

	bool fullScreen = fullscreen();

	std::vector<std::pair<int,int> > resolutions;
	int display = SDL_GetWindowDisplayIndex(disp.video().getWindow());
	int modes = SDL_GetNumDisplayModes(display);
	SDL_DisplayMode mode = {SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };
	SDL_Rect screen_rect = disp.video().bound();
	for (int n = 0; n < modes; n ++) {
		SDL_GetDisplayMode(display, n, &mode);
		if (SDL_BYTESPERPIXEL(mode.format) != SDL_BYTESPERPIXEL(video.getformat())) {
			continue;
		}
#if defined(_WIN32)
		if (!fullScreen && (mode.w >= screen_rect.w || mode.h >= screen_rect.h)) {
			continue;
		}
#endif
		if (mode.w >= min_allowed_width() && mode.h >= min_allowed_height()) {
			resolutions.push_back(std::pair<int, int>(mode.w, mode.h));
		}
	}

	if (resolutions.empty()) {
		gui2::show_transient_message(disp.video(), "", _("There are no alternative video modes available"));
		return false;
	}

	const std::pair<int,int> current_res(video.getSurface()->w, video.getSurface()->h);
	resolutions.push_back(current_res);
	if (!fullScreen) {
		resolutions.push_back(std::make_pair(480, 320));
		resolutions.push_back(std::make_pair(568, 320));
		resolutions.push_back(std::make_pair(800, 480));
		resolutions.push_back(std::make_pair(854, 480));
		resolutions.push_back(std::make_pair(1280, 720));
		resolutions.push_back(std::make_pair(1280, 800));
	}

	std::sort(resolutions.begin(),resolutions.end(),compare_resolutions);
	resolutions.erase(std::unique(resolutions.begin(),resolutions.end()),resolutions.end());

	std::vector<std::string> options;
	unsigned current_choice = 0;

	for(size_t k = 0; k < resolutions.size(); ++k) {
		std::pair<int, int> const& res = resolutions[k];
		std::ostringstream option;

		if (res == current_res) {
			current_choice = static_cast<unsigned>(k);		
		}

		option << res.first << "x" << res.second;
		// widescreen threshold is 16:10
		if ((double)res.first/res.second >= 16.0/10.0) {
			option << _(" (widescreen)");
		}
		options.push_back(option.str());
	}

	gui2::tsimple_item_selector dlg(_("Choose Resolution"), "", options);
	dlg.set_selected_index(current_choice);
	dlg.show(disp.video());

	int choice = dlg.selected_index();

	if (choice == -1 || resolutions[static_cast<size_t>(choice)] == current_res) {
		return false;
	}

	std::pair<int, int>& res = resolutions[static_cast<size_t>(choice)];
	set_resolution(disp, res.first, res.second);
	return true;
}

void show_preferences_dialog(display& disp)
{
	bool first = true;
	while (true) {
		int res = gui2::app_show_preferences_dialog(disp, first);
		if (first) {
			first = false;
		}
		if (res == preferences::CHANGE_RESOLUTION) {
			if (preferences::show_video_mode_dialog(disp)) {
				return;
			}

		} else if (res == preferences::MAKE_FULLSCREEN) {
			preferences::set_fullscreen(disp, true);
			return;

		} else if (res == preferences::MAKE_WINDOWED) {
			preferences::set_fullscreen(disp, false);
			return;

		} else {
			return;
		}
	}
}

} // end namespace preferences

