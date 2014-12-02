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

// #include "construct_dialog.hpp"
#include "display.hpp"
#include "preferences.hpp"
#include "gettext.hpp"
#include "gui/dialogs/simple_item_selector.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "hotkeys.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "wml_separators.hpp"

#include <boost/foreach.hpp>

namespace preferences {

display* disp = NULL;

display_manager::display_manager(display* d)
{
	disp = d;

	load_hotkeys();

	set_grid(grid());
	set_turbo(turbo());
	set_turbo_speed(turbo_speed());
	set_fullscreen(fullscreen());
	set_scroll_to_action(scroll_to_action());
}

display_manager::~display_manager()
{
	disp = NULL;
}

bool detect_video_settings(CVideo& video, std::pair<int,int>& resolution, int& bpp, int& video_flags)
{
	video_flags = fullscreen() ? SDL_WINDOW_FULLSCREEN: 0;
	resolution = preferences::resolution();

	bpp = 32;
	return true;
/*
	int DefaultBPP = 24;
	const SDL_VideoInfo* const video_info = SDL_GetVideoInfo();
	if(video_info != NULL && video_info->vfmt != NULL) {
		DefaultBPP = video_info->vfmt->BitsPerPixel;
	}

	std::cerr << "Checking video mode: " << resolution.first << 'x'
		<< resolution.second << 'x' << DefaultBPP << "...\n";

	typedef std::pair<int, int> res_t;
	std::vector<res_t> res_list;
	res_list.push_back(res_t(1024, 768));
	res_list.push_back(res_t(1024, 600));
	res_list.push_back(res_t(800, 600));
	res_list.push_back(res_t(480, 320));

	bpp = video.modePossible(resolution.first, resolution.second,
		DefaultBPP, video_flags, true);

	BOOST_FOREACH (const res_t &res, res_list)
	{
		if (bpp != 0) break;
		std::cerr << "Video mode " << resolution.first << 'x'
			<< resolution.second << 'x' << DefaultBPP
			<< " is not supported; attempting " << res.first
			<< 'x' << res.second << 'x' << DefaultBPP << "...\n";
		resolution = res;
		bpp = video.modePossible(resolution.first, resolution.second,
			DefaultBPP, video_flags);
	}

	return bpp != 0;
*/
}

void set_fullscreen(CVideo& video, const bool ison)
{
	_set_fullscreen(ison);

	const std::pair<int,int>& res = resolution();
	if(video.isFullScreen() != ison) {
		const int flags = ison ? SDL_WINDOW_FULLSCREEN: 0;
		int bpp = video.modePossible(res.first,res.second,32,flags);
		if (bpp <= 0) {
			bpp = video.modePossible(res.first,res.second,16,flags);
		}

		if(bpp > 0) {
			video.setMode(res.first,res.second,bpp,flags);
			if(disp) {
				disp->redraw_everything();
			}
		} else {
			int tmp_flags = flags;
			std::pair<int,int> tmp_res;
			if(detect_video_settings(video, tmp_res, bpp, tmp_flags)) {
				set_resolution(video, tmp_res.first, tmp_res.second);
			// TODO: see if below line is actually needed, possibly for displays that only support 16 bbp
			} else if(video.modePossible(1024,768,16,flags)) {
				set_resolution(video, 1024, 768);
			} else {
				gui2::show_transient_message(video,"",_("The video mode could not be changed. Your window manager must be set to 16 bits per pixel to run the game in windowed mode. Your display must support 1024x768x16 to run the game full screen."));
			}
		}
	}
}

void set_fullscreen(bool ison)
{
	_set_fullscreen(ison);

	if(disp != NULL) {
		set_fullscreen(disp->video(), ison);
	}
}

void set_scroll_to_action(bool ison)
{
	_set_scroll_to_action(ison);
}
void set_resolution(const std::pair<int,int>& resolution)
{
	if (disp) {
		set_resolution(disp->video(), resolution.first, resolution.second);
	} else {
		/* This part is needed when wesnoth is started with the -r parameter. */
		const std::string postfix = fullscreen() ? "resolution" : "windowsize";
		preferences::set('x' + postfix, lexical_cast<std::string>(resolution.first));
		preferences::set('y' + postfix, lexical_cast<std::string>(resolution.second));
	}
}

bool set_resolution(CVideo& video
		, const unsigned width, const unsigned height)
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
	SDL_GetClipRect(video.getSurface(), &rect);
	if(rect.w == width && rect.h == height) {
		return true;
	}

	const int flags = fullscreen() ? SDL_WINDOW_FULLSCREEN : 0;
	int bpp = video.modePossible(width, height, 32, flags);
	if (bpp == 0) {
		bpp = video.modePossible(width, height, 16, flags);
	}
	if (bpp != 0) {
		video.setMode(width, height, bpp, flags);

		if (disp) {
			disp->redraw_everything();
		}

	} else {
		gui2::show_transient_message(video,"",_("The video mode could not be changed. Your window manager must be set to 16 bits per pixel to run the game in windowed mode. Your display must support 1024x768x16 to run the game full screen."));
		return false;
	}

	if (width < 480 || height < 320) {
		return false;
	}
	const std::string postfix = fullscreen() ? "resolution" : "windowsize";
	preferences::set('x' + postfix, lexical_cast<std::string>(width));
	preferences::set('y' + postfix, lexical_cast<std::string>(height));
	
	return true;
}

void set_turbo(bool ison)
{
	_set_turbo(ison);

	if(disp != NULL) {
		disp->set_turbo(ison);
	}
}

void set_turbo_speed(double speed)
{
	save_turbo_speed(speed);

	if(disp != NULL) {
		disp->set_turbo_speed(speed);
	}
}

void set_grid(bool ison)
{
	_set_grid(ison);

	if(disp != NULL) {
		disp->set_grid(ison);
	}
}

void set_default_move(bool ison) 
{
	_set_default_move(ison);
}

namespace {
class escape_handler : public events::handler {
public:
	escape_handler() : escape_pressed_(false) {}
	bool escape_pressed() const { return escape_pressed_; }
	void handle_event(const SDL_Event &event) { escape_pressed_ |= (event.type == SDL_KEYDOWN)
		&& (reinterpret_cast<const SDL_KeyboardEvent&>(event).keysym.sym == SDLK_ESCAPE); }
private:
	bool escape_pressed_;
};
} // end anonymous namespace

bool compare_resolutions(const std::pair<int,int>& lhs, const std::pair<int,int>& rhs)
{
	return lhs.first*lhs.second < rhs.first*rhs.second;
}

bool show_video_mode_dialog(display& disp)
{
	const resize_lock prevent_resizing;
	const events::event_context dialog_events_context;

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
	}

	std::sort(resolutions.begin(),resolutions.end(),compare_resolutions);
	resolutions.erase(std::unique(resolutions.begin(),resolutions.end()),resolutions.end());

	std::vector<std::string> options;
	unsigned current_choice = 0;

	for(size_t k = 0; k < resolutions.size(); ++k) {
		std::pair<int, int> const& res = resolutions[k];
		std::ostringstream option;

		if (res == current_res)
			current_choice = static_cast<unsigned>(k);		

		option << res.first << "x" << res.second;
		// widescreen threshold is 16:10
		if ((double)res.first/res.second >= 16.0/10.0)
		  option << _(" (widescreen)");
		options.push_back(option.str());
	}

	gui2::tsimple_item_selector dlg(_("Choose Resolution"), "", options);
	dlg.set_selected_index(current_choice);
	dlg.show(disp.video());

	int choice = dlg.selected_index();

	if (choice == -1 || resolutions[static_cast<size_t>(choice)] == current_res) {
		return false;
	}

	set_resolution(resolutions[static_cast<size_t>(choice)]);
	return true;
}

} // end namespace preferences

