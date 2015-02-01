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

#include "gui/auxiliary/event/handler.hpp"
#include "display.hpp"
#include "preferences.hpp"
#include "mouse_handler_base.hpp"
#include "hotkeys.hpp"

#include <boost/foreach.hpp>

controller_base::controller_base(int ticks, const config& game_config, CVideo& /*video*/)
	: events::handler(false)
	, game_config_(game_config)
	, ticks_(ticks)
	, key_()
	, browse_(false)
	, scrolling_(false)
	, finger_motion_scroll_(false)
	, finger_motion_direction_(UP)
	, wait_bh_event_(false)
{
}

controller_base::~controller_base()
{
}

int controller_base::get_ticks() {
	return ticks_;
}

bool controller_base::handle_scroll_wheel(int dx, int dy, int hit_threshold, int motion_threshold)
{
	int abs_dx = abs(dx);
	int abs_dy = abs(dy);
	if (abs_dx <= hit_threshold && abs_dy <= hit_threshold) {
		return false;
	}
	if (abs_dx >= motion_threshold && abs_dy >= motion_threshold) {
		if (dx > 0) {
			if (dy > 0) {
				finger_motion_direction_ = SOUTH_EAST;
			} else {
				finger_motion_direction_ = NORTH_EAST;
			}
		} else if (dy >= 0) {
			finger_motion_direction_ = SOUTH_WEST;
		} else {
			finger_motion_direction_ = NORTH_WEST;
		}
		finger_motion_scroll_ = true;
	} else if (abs_dx >= abs_dy && abs_dx >= motion_threshold) {
		// x axis
		if (dx > 0) {
			finger_motion_direction_ = RIGHT;
		} else {
			finger_motion_direction_ = LEFT;
		}
		finger_motion_scroll_ = true;
	} else if (abs_dx < abs_dy && abs_dy >= motion_threshold) {
		// y axis
		if (dy > 0) {
			finger_motion_direction_ = DOWN;
		} else {
			finger_motion_direction_ = UP;
		}
		finger_motion_scroll_ = true;
	}
	return true;
}

void controller_base::handle_event(const SDL_Event& event)
{
	if (gui2::is_in_dialog()) {
		return;
	}

	display& disp = get_display();
	CVideo& video = disp.video();

	SDL_Event new_event;
	// events::mouse_handler& mouse_handler = get_mouse_handler_base();
	int x, y, dx, dy;

	switch(event.type) {
	case SDL_FINGERDOWN:
		if (events::ignore_finger_event) {
			break;
		}
		wait_bh_event_ = true;
		break;

	case SDL_KEYDOWN:
		// Detect key press events, unless there something that has keyboard focus
		// in which case the key press events should go only to it.
		if(have_keyboard_focus()) {
			process_keydown_event(event);
			// hotkey::key_event(get_display(),event.key,this);
		} else {
			process_focus_keydown_event(event);
			break;
		}
		// intentionally fall-through
	case SDL_KEYUP:
		process_keyup_event(event);
		break;

	case SDL_FINGERMOTION:
		if (events::ignore_finger_event) {
			break;
		}
		x = event.tfinger.x * video.getx();
		y = event.tfinger.y * video.gety();
		dx = event.tfinger.dx * video.getx();
		dy = event.tfinger.dy * video.gety();

		if (!handle_scroll_wheel(dx, dy, FINGER_HIT_THRESHOLD, FINGER_MOTION_THRESHOLD)) {
			break;
		}
		wait_bh_event_ = false;
		events::ignore_finger_event = true;
		break;

	case SDL_MOUSEMOTION:
		if (event.button.which == SDL_TOUCH_MOUSEID) {
			break;
		}
		// Ignore old mouse motion events in the event queue
		if (SDL_PeepEvents(&new_event,1,SDL_GETEVENT, SDL_MOUSEMOTIONMASK) > 0) {
			while(SDL_PeepEvents(&new_event,1,SDL_GETEVENT, SDL_MOUSEMOTIONMASK) > 0) {};
			get_mouse_handler_base().mouse_motion_event(new_event.motion, browse_);
		} else {
			get_mouse_handler_base().mouse_motion_event(event.motion, browse_);
		}
		break;
	case SDL_FINGERUP:
		if (events::ignore_finger_event) {
			break;
		}
		if (!wait_bh_event_) {
			break;
		}
		x = event.tfinger.x * video.getx();
		y = event.tfinger.y * video.gety();
		// simulate SDL_MOUSEBUTTONDOWN
		new_event = event;
		new_event.button.button = SDL_BUTTON_LEFT;
		new_event.button.state = SDL_PRESSED;
		new_event.button.x = x;
		new_event.button.y = y;
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

	case SDL_MOUSEWHEEL:
		if (event.wheel.which == SDL_TOUCH_MOUSEID) {
			break;
		}
		{
			int x, y;
			SDL_GetMouseState(&x, &y);
			if (!point_in_rect(x, y, disp.map_outside_area())) {
				break;
			}
			handle_scroll_wheel(event.wheel.x, event.wheel.y, MOUSE_HIT_THRESHOLD, MOUSE_MOTION_THRESHOLD);
		}
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (event.button.which == SDL_TOUCH_MOUSEID) {
			break;
		}
		get_mouse_handler_base().mouse_press(event.button, browse_);
		post_mouse_press(event);
		if (get_mouse_handler_base().get_show_menu()){
			get_display().goto_main_context_menu();
		}
		break;

	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
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
	bool mouse_in_window = (SDL_GetAppState(get_display().video().getWindow()) & SDL_APPMOUSEFOCUS) != 0;
#endif
	
	bool keyboard_focus = have_keyboard_focus();
	int scroll_speed = preferences::scroll_speed();
	int dx = 0, dy = 0;
	int scroll_threshold = (preferences::mouse_scroll_enabled())
		? preferences::mouse_scroll_threshold() : 0;
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

	int mousex, mousey;
	Uint8 mouse_flags = SDL_GetMouseState(&mousex, &mousey);
	bool was_scrolling = scrolling_;
	scrolling_ = handle_scroll(key, mousex, mousey, mouse_flags);

	get_display().draw();

	// be nice when window is not visible
	// NOTE should be handled by display instead, to only disable drawing
	if (is_delay_enabled && (SDL_GetAppState(gui.video().getWindow()) & SDL_APPACTIVE) == 0) {
		get_display().delay(200);
	}

	if (!scrolling_ && was_scrolling) {
#if (defined(__APPLE__) && TARGET_OS_IPHONE) || defined(ANDROID)
	
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

const config& controller_base::get_theme(const config& game_config, std::string theme_name)
{
	if (const config &c = game_config.find_child("theme", "name", theme_name))
		return c;

	if (const config &c = game_config.find_child("theme", "name", "Default"))
		return c;

	static config empty;
	return empty;
}

bool controller_base::actived_context_menu(const std::string& id) const
{ 
	const display& disp = get_display();
	std::pair<std::string, std::string> item = gui2::tcontext_menu::extract_item(id);
	int command = hotkey::get_hotkey(item.first).get_id();

	int zoom = disp.hex_width();

	switch (command) {
	case HOTKEY_ZOOM_IN:
		return zoom < disp.max_zoom();
	case HOTKEY_ZOOM_OUT:
		return zoom > disp.min_zoom();
	}

	return true; 
}

void controller_base::execute_command(int command, const std::string& sparam)
{
	if (!can_execute_command(command, sparam)) {
		return;
	}
	return execute_command2(command, sparam);
}

void controller_base::execute_command2(int command, const std::string& sparam)
{
	const int zoom_amount = 4;
	display& disp = get_display();

	switch(command) {
	case HOTKEY_SYSTEM:
		// system();
		return;

	case HOTKEY_ZOOM_IN:
		disp.set_zoom(zoom_amount);
		return;

	case HOTKEY_ZOOM_OUT:
		disp.set_zoom(-zoom_amount);
		return;

	case HOTKEY_ZOOM_DEFAULT:
		disp.set_default_zoom();
		return;
	}
}