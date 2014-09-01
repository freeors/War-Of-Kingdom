/* $Id: button.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/button.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/button.hpp"
#include "gui/auxiliary/window_builder/button.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "sound.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(button)

tbutton::tbutton()
	: tcontrol(COUNT)
	, tclickable_()
	, state_(ENABLED)
	, sort_(NONE)
	, sound_button_click_()
	, retval_(0)
{
	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&tbutton::signal_handler_mouse_enter, this, _2, _3));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&tbutton::signal_handler_mouse_leave, this, _2, _3));

	connect_signal<event::LEFT_BUTTON_DOWN>(boost::bind(
				&tbutton::signal_handler_left_button_down, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_UP>(boost::bind(
				&tbutton::signal_handler_left_button_up, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				&tbutton::signal_handler_left_button_click, this, _2, _3));
}

void tbutton::load_config_extra()
{
	BOOST_FOREACH(tcanvas& canva, canvas()) {
		canva.set_variable("sort", variant(sort_));
		canva.set_variable("sort_image", variant(""));
	}
}

void tbutton::set_active(const bool active)
{ 
	if (get_active() != active) {
		set_state(active ? ENABLED : DISABLED); 
	}
};

void tbutton::set_state(const tstate state)
{
	if (state_ == DISABLED && state != ENABLED) {
		// reference http://www.freeors.com/bbs/forum.php?mod=viewthread&tid=21966&extra=page%3D1
		return;
	}

	if (state != state_) {
		state_ = state;
		set_dirty(true);
	}
}

void tbutton::set_sort(tsort sort)
{
	if (sort == sort_) {
		return;
	}
	if (sort == TOGGLE) {
		if (sort_ == NONE || sort_ == DESCEND) {
			sort_ = ASCEND;
		} else if (sort_ == ASCEND) {
			sort_ = DESCEND;
		}
	} else {
		sort_ = sort;
	}
	std::string name;

	if (sort_ == ASCEND) {
		name = "buttons/sort-ascend.png";
	} else if (sort_ == DESCEND) {
		name = "buttons/sort-descend.png";
	}

	BOOST_FOREACH(tcanvas& canva, canvas()) {
		canva.set_variable("sort", variant(sort_));
		canva.set_variable("sort_image", variant(name));
	}

	set_dirty();
}

int tbutton::get_sort() const
{
	return sort_;
}

const std::string& tbutton::get_control_type() const
{
	static const std::string type = "button";
	return type;
}

void tbutton::signal_handler_mouse_enter(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!get_active()) {
		handled = true;
		return;
	}
	set_state(FOCUSSED);
	handled = true;
}

void tbutton::signal_handler_mouse_leave(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!get_active()) {
		handled = true;
		return;
	}
	set_state(ENABLED);
	handled = true;
}

void tbutton::signal_handler_left_button_down(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	twindow* window = get_window();
	if(window) {
		window->mouse_capture();
	}

	set_state(PRESSED);
	handled = true;
}

void tbutton::signal_handler_left_button_up(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	set_state(FOCUSSED);
	handled = true;
}

void tbutton::signal_handler_left_button_click(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	if (sound_button_click_.empty()) {
		sound::play_UI_sound(settings::sound_button_click);
	} else {
		sound::play_UI_sound(sound_button_click_);
	}

	// If a button has a retval do the default handling.
	if(retval_ != 0) {
		twindow* window = get_window();
		if(window) {
			window->set_retval(retval_);
			return;
		}
	}

	handled = true;
}

} // namespace gui2
