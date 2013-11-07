/* $Id: controller_base.cpp 47506 2010-11-07 20:19:57Z silene $ */
/*
   Copyright (C) 2006 - 2010 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "controller_base.hpp"

#include "dialogs.hpp"
#include "display.hpp"
#include "game_preferences.hpp"
#include "log.hpp"
#include "mouse_handler_base.hpp"
#include "mouse_events.hpp"
#include "sound.hpp"
#include "resources.hpp"

#include <boost/foreach.hpp>

static lg::log_domain log_display("display");
#define ERR_DP LOG_STREAM(err, log_display)

controller_base::controller_base(
		int ticks, const config& game_config, CVideo& /*video*/) :
	game_config_(game_config),
	ticks_(ticks),
	key_(),
	browse_(false),
	scrolling_(false),
	finger_motion_scroll_(false),
	finger_motion_direction_(UP),
	wait_bh_event_(false)
{
}

controller_base::~controller_base()
{
}

int controller_base::get_ticks() {
	return ticks_;
}

#define FINGER_MOTION_THRESHOLD		10
#define FINGER_HIT_THRESHOLD     4
void controller_base::handle_event(const SDL_Event& event)
{
	if (gui::in_dialog()) {
		return;
	}

#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	if ((event.type == SDL_MOUSEBUTTONDOWN) || (event.type == SDL_FINGERDOWN)) {
		wait_bh_event_ = true;
		return;
	}
	if (event.type == SDL_MOUSEMOTION) {
		return;
	}
	if (event.type == SDL_MOUSEBUTTONUP) {
		return;
	}
#endif
	SDL_Event new_event;
	int abs_dx, abs_dy;
	// events::mouse_handler& mouse_handler = get_mouse_handler_base();

	switch(event.type) {
	case SDL_KEYDOWN:
		// Detect key press events, unless there something that has keyboard focus
		// in which case the key press events should go only to it.
		if(have_keyboard_focus()) {
			process_keydown_event(event);
			hotkey::key_event(get_display(),event.key,this);
		} else {
			process_focus_keydown_event(event);
			break;
		}
		// intentionally fall-through
	case SDL_KEYUP:
		process_keyup_event(event);
		break;

	case SDL_FINGERMOTION:
		abs_dx = posix_abs(event.tfinger.dx);
		abs_dy = posix_abs(event.tfinger.dy);
		if (abs_dx <= FINGER_HIT_THRESHOLD && abs_dy <= FINGER_HIT_THRESHOLD) {
			break;
		}
		if (abs_dx >= FINGER_MOTION_THRESHOLD && abs_dy >= FINGER_MOTION_THRESHOLD) {
			if (event.tfinger.dx > 0) {
				if (event.tfinger.dy > 0) {
					finger_motion_direction_ = SOUTH_EAST;
				} else {
					finger_motion_direction_ = NORTH_EAST;
				}
			} else if (event.tfinger.dy >= 0) {
				finger_motion_direction_ = SOUTH_WEST;
			} else {
				finger_motion_direction_ = NORTH_WEST;
			}
			finger_motion_scroll_ = true;
		} else if (abs_dx >= abs_dy && abs_dx >= FINGER_MOTION_THRESHOLD) {
			// x axis
			if (event.tfinger.dx > 0) {
				finger_motion_direction_ = RIGHT;
			} else {
				finger_motion_direction_ = LEFT;
			}
			finger_motion_scroll_ = true;
		} else if (abs_dx < abs_dy && abs_dy >= FINGER_MOTION_THRESHOLD) {
			// y axis
			if (event.tfinger.dy > 0) {
				finger_motion_direction_ = DOWN;
			} else {
				finger_motion_direction_ = UP;
			}
			finger_motion_scroll_ = true;
		}
		wait_bh_event_ = false;
		break;

	case SDL_MOUSEMOTION:
		// Ignore old mouse motion events in the event queue
		if (SDL_PeepEvents(&new_event,1,SDL_GETEVENT, SDL_MOUSEMOTIONMASK) > 0) {
			while(SDL_PeepEvents(&new_event,1,SDL_GETEVENT, SDL_MOUSEMOTIONMASK) > 0) {};
			get_mouse_handler_base().mouse_motion_event(new_event.motion, browse_);
		} else {
			get_mouse_handler_base().mouse_motion_event(event.motion, browse_);
		}
		break;
	case SDL_FINGERUP:
		if (!wait_bh_event_) {
			break;
		}
		// simulate SDL_MOUSEBUTTONDOWN
		new_event = event;
		new_event.button.button = SDL_BUTTON_LEFT;
		new_event.button.state = SDL_PRESSED;
		new_event.button.x = event.tfinger.x;
		new_event.button.y = event.tfinger.y;
		// SDL_SendMouseMotion(NULL, 0, event.tfinger.x, event.tfinger.y);
		do {
			get_mouse_handler_base().mouse_press(new_event.button, browse_);
			post_mouse_press(new_event);
			if (get_mouse_handler_base().get_show_menu()){
				get_display().goto_main_context_menu();
			}
			if (new_event.button.state == SDL_RELEASED) {
				break;
			}
			new_event.button.state = SDL_RELEASED;
		} while (true);
		wait_bh_event_ = false;
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		get_mouse_handler_base().mouse_press(event.button, browse_);
		post_mouse_press(event);
		if (get_mouse_handler_base().get_show_menu()){
			get_display().goto_main_context_menu();
		}
		break;
	case SDL_ACTIVEEVENT:
		if (event.active.type == SDL_APPMOUSEFOCUS && event.active.gain == 0) {
			if (get_mouse_handler_base().is_dragging()) {
				//simulate mouse button up when the app has lost mouse focus
				//this should be a general fix for the issue when the mouse
				//is dragged out of the game window and then the button is released
				int x, y;
				Uint8 mouse_flags = SDL_GetMouseState(&x, &y);
				if ((mouse_flags & SDL_BUTTON_LEFT) == 0) {
					SDL_Event e;
					e.type = SDL_MOUSEBUTTONUP;
					e.button.state = SDL_RELEASED;
					e.button.button = SDL_BUTTON_LEFT;
					e.button.x = x;
					e.button.y = y;
					get_mouse_handler_base().mouse_press(event.button, browse_);
					post_mouse_press(event);
				}
			}
		}
		break;
	default:
		break;
	}
}

bool controller_base::have_keyboard_focus()
{
	return true;
}

void controller_base::process_focus_keydown_event(const SDL_Event& /*event*/) {
	//no action by default
}

void controller_base::process_keydown_event(const SDL_Event& /*event*/) {
	//no action by default
}

void controller_base::process_keyup_event(const SDL_Event& /*event*/) {
	//no action by default
}

void controller_base::post_mouse_press(const SDL_Event& /*event*/) {
	//no action by default
}

bool controller_base::handle_scroll(CKey& key, int mousex, int mousey, int mouse_flags)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	// for tablet device.
	bool mouse_in_window = false;
#else
	// for mouse device.
	bool mouse_in_window = (SDL_GetAppState() & SDL_APPMOUSEFOCUS) != 0
		|| preferences::get("scroll_when_mouse_outside", true);
#endif
	
	bool keyboard_focus = have_keyboard_focus();
	int scroll_speed = preferences::scroll_speed();
	int dx = 0, dy = 0;
	int scroll_threshold = (preferences::mouse_scroll_enabled())
		? preferences::mouse_scroll_threshold() : 0;
	BOOST_FOREACH (const theme::menu& m, get_display().get_theme().menus()) {
		if (point_in_rect(mousex, mousey, m.get_location())) {
			scroll_threshold = 0;
		}
	}
	if ((key[SDLK_UP] && keyboard_focus) ||
	    (mousey < scroll_threshold && mouse_in_window))
	{
		dy -= scroll_speed;
	}
	if ((key[SDLK_DOWN] && keyboard_focus) ||
	    (mousey > get_display().h() - scroll_threshold && mouse_in_window))
	{
		dy += scroll_speed;
	}
	if ((key[SDLK_LEFT] && keyboard_focus) ||
	    (mousex < scroll_threshold && mouse_in_window))
	{
		dx -= scroll_speed;
	}
	if ((key[SDLK_RIGHT] && keyboard_focus) ||
	    (mousex > get_display().w() - scroll_threshold && mouse_in_window))
	{
		dx += scroll_speed;
	}
	if ((mouse_flags & SDL_BUTTON_MMASK) != 0 && preferences::middle_click_scrolls()) {
		const SDL_Rect& rect = get_display().map_outside_area();
		if (point_in_rect(mousex, mousey,rect)) {
			// relative distance from the center to the border
			// NOTE: the view is a rectangle, so can be more sensible in one direction
			// but seems intuitive to use and it's useful since you must
			// more often scroll in the direction where the view is shorter
			const double xdisp = ((1.0*mousex / rect.w) - 0.5);
			const double ydisp = ((1.0*mousey / rect.h) - 0.5);
			// 4.0 give twice the normal speed when mouse is at border (xdisp=0.5)
			int speed = 4 * scroll_speed;
			dx += round_double(xdisp * speed);
			dy += round_double(ydisp * speed);
		}
	}
	if (finger_motion_scroll_) {
		if (finger_motion_direction_ == UP) {
			dy += scroll_speed;
		} else if (finger_motion_direction_ == DOWN) {
			dy -= scroll_speed;
		} else if (finger_motion_direction_ == LEFT) {
			dx += scroll_speed;
		} else if (finger_motion_direction_ == RIGHT) {
			dx -= scroll_speed;
		} else if (finger_motion_direction_ == NORTH_EAST) {
			dx -= scroll_speed;
			dy += scroll_speed;
		} else if (finger_motion_direction_ == SOUTH_EAST) {
			dx -= scroll_speed;
			dy -= scroll_speed;
		} else if (finger_motion_direction_ == SOUTH_WEST) {
			dx += scroll_speed;
			dy -= scroll_speed;
		} else if (finger_motion_direction_ == NORTH_WEST) {
			dx += scroll_speed;
			dy += scroll_speed;
		} 
		finger_motion_scroll_ = false;
	}
	return get_display().scroll(dx, dy);
}

void controller_base::play_slice(bool is_delay_enabled)
{
	display& gui = get_display();

	CKey key;
	events::pump();
	events::raise_process_event();
	events::raise_draw_event();

	slice_before_scroll();
// const theme::menu* const m = get_display().menu_pressed();
	const button_loc& loc = get_display().menu_pressed();
	const theme::menu* m = loc.first;
	if (m && (m == gui.access_troop_menu())) {
		map_location pressed_loc = gui.access_list_press(loc.second);
		events::mouse_handler& m_handler = *events::mouse_handler::get_singleton();
		if (pressed_loc.x == MAGIC_HERO && pressed_loc.y >= 0) {
			hero& h = (*resources::heros)[pressed_loc.y];
			m_handler.set_hero_placing(&h);
			
		} else if (pressed_loc.valid() && !m_handler.in_multistep_state()) {
			gui.scroll_to_tile(pressed_loc, display::WARP);
			m_handler.select_hex(map_location(), false);
			// gui.select_hex(pressed_loc);
			// sound::play_UI_sound("select-unit.wav");
			resources::units->find(pressed_loc)->set_selecting();
			resources::screen->invalidate_unit();
			// now, selectedHex_ is invalid, hide context menu.
			gui.goto_main_context_menu();			
		}
	} else if (m != NULL){
		const SDL_Rect& menu_loc = m->location(get_display().screen_area());
		if (m->is_context()) {
			// Is pressed menu a father menu of context-menu?
			std::string item = m->items()[loc.second];
			bool executed = false;
			while (!executed) {
				std::vector<std::string> item2 = utils::split(item, ':');
				if (item2.size() == 1) {
					// item2.push_back(item2[0]) is wrong way, resulting item2[1] is null string.
					item2.push_back("");
				}
				size_t pos = item2[0].rfind("_m");
				if (pos == item2[0].size() - 2) {
					// cancel current menu, and display sub-menu
					gui.hide_context_menu(NULL, true);
					const std::string item1 = item2[0].substr(0, pos);
					gui.get_theme().set_current_context_menu(get_display().get_theme().context_menu(item1));
					show_context_menu(NULL, get_display());
					executed = true;
				} else {
					// execute one menu command
					pos = item2[0].rfind("_c");
					if (pos == item2[0].size() - 2) {
						const std::string item1 = item2[0].substr(0, pos);
						if (item2[1].rfind("_m") != item2[1].size() - 2) {
							execute_command(hotkey::get_hotkey(item1).get_id(), -1, item2[1]);
							gui.hide_context_menu(NULL, true);
							executed = true;
						} else {
							item = item2[1];
							continue;
						}
					} else {
						execute_command(hotkey::get_hotkey(item2[0]).get_id(), -1, item2[1]);
						executed = true;
					}
				}
			}
		} else {
			show_menu(m->items(), menu_loc.x+1, menu_loc.y + menu_loc.h + 1, false);
		}
		return;
	}

	int mousex, mousey;
	Uint8 mouse_flags = SDL_GetMouseState(&mousex, &mousey);
	bool was_scrolling = scrolling_;
	scrolling_ = handle_scroll(key, mousex, mousey, mouse_flags);

	get_display().draw();

	// be nice when window is not visible
	// NOTE should be handled by display instead, to only disable drawing
	if (is_delay_enabled && (SDL_GetAppState() & SDL_APPACTIVE) == 0) {
		get_display().delay(200);
	}

	if (!scrolling_ && was_scrolling) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
		// swip result to scroll, don't update mouse hex.
		SDL_Event new_event;
		while(SDL_PeepEvents(&new_event, 1, SDL_GETEVENT, SDL_FINGERMOTION, SDL_FINGERMOTION) > 0) {
			posix_print("SDL_FINGERMOTION(discard), (x, y): (%u, %u), (dx, dy): (%i, %i)\n", 
				new_event.tfinger.x, new_event.tfinger.y, new_event.tfinger.dx, new_event.tfinger.dy);
		};
#else
		// scrolling ended, update the cursor and the brightened hex
		get_mouse_handler_base().mouse_update(browse_);
#endif
	}
	slice_end();
}

void controller_base::slice_before_scroll()
{
	//no action by default
}

void controller_base::slice_end()
{
	//no action by default
}

void controller_base::show_menu(const std::vector<std::string>& items_arg, int xloc, int yloc, bool context_menu)
{
}

void controller_base::show_context_menu(theme::menu* m, display& gui)
{
}

bool controller_base::in_context_menu(hotkey::HOTKEY_COMMAND /*command*/) const
{
	return true;
}

const config& controller_base::get_theme(const config& game_config, std::string theme_name)
{
	if (theme_name.empty()) theme_name = preferences::theme();

	if (const config &c = game_config.find_child("theme", "name", theme_name))
		return c;

	ERR_DP << "Theme '" << theme_name << "' not found. Trying the default theme.\n";

	if (const config &c = game_config.find_child("theme", "name", "Default"))
		return c;

	ERR_DP << "Default theme not found.\n";

	static config empty;
	return empty;
}
