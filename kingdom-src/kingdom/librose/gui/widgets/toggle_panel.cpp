/* $Id: toggle_panel.cpp 54604 2012-07-07 00:49:45Z loonycyborg $ */
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

#include "gui/widgets/toggle_panel.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/toggle_panel.hpp"
#include "gui/auxiliary/window_builder/toggle_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "sound.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(toggle_panel)

ttoggle_panel::ttoggle_panel()
	: tpanel(COUNT + 1)
	, state_(ENABLED)
	, retval_(0)
	, callback_state_change_(0)
	, callback_mouse_left_double_click_()
{
	set_wants_mouse_left_double_click();

	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_enter, this, _2, _3));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttoggle_panel::signal_handler_mouse_leave, this, _2, _3));

	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_pre_left_button_click
				, this, _2)
			, event::tdispatcher::back_pre_child);
	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				&ttoggle_panel::signal_handler_left_button_click
					, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_click
				, this, _2, _3)
			, event::tdispatcher::back_post_child);
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_double_click
				, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_DOUBLE_CLICK>(boost::bind(
				  &ttoggle_panel::signal_handler_left_button_double_click
				, this, _2, _3)
			, event::tdispatcher::back_post_child);
}

void ttoggle_panel::set_child_members(const std::map<std::string /* widget id */, string_map>& data)
{
	// typedef boost problem work around.
	typedef std::pair<std::string, string_map> hack ;
	BOOST_FOREACH(const hack& item, data) {
		tcontrol* control = dynamic_cast<tcontrol*>(find(item.first, false));
		if(control) {
			control->set_members(item.second);
		}
	}
}

void ttoggle_panel::child_populate_dirty_list(twindow& caller, const std::vector<twidget*>& call_stack)
{
	std::vector<std::vector<twidget*> >& dirty_list = caller.dirty_list();
	size_t dirty_size = dirty_list.size();

	tcontainer_::child_populate_dirty_list(caller, call_stack);
	if (dirty_list.size() != dirty_size) {
		for (std::vector<std::vector<twidget*> >::iterator i = dirty_list.begin() + dirty_size; i != dirty_list.end(); ) {
			i = dirty_list.erase(i);
		}
		caller.add_to_dirty_list(call_stack);
	} 
}

void ttoggle_panel::set_active(const bool active)
{
	if(active) {
		if(get_value()) {
			set_state(ENABLED_SELECTED);
		} else {
			set_state(ENABLED);
		}
	} else {
		if(get_value()) {
			set_state(DISABLED_SELECTED);
		} else {
			set_state(DISABLED);
		}
	}
}

SDL_Rect ttoggle_panel::get_client_rect() const
{
	boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(config());
	assert(conf);

	SDL_Rect result = get_rect();
	result.x += conf->left_border;
	result.y += conf->top_border;
	result.w -= conf->left_border + conf->right_border;
	result.h -= conf->top_border + conf->bottom_border;

	return result;
}

tpoint ttoggle_panel::border_space() const
{
	boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(config());
	assert(conf);

	return tpoint(conf->left_border + conf->right_border,
		conf->top_border + conf->bottom_border);
}

void ttoggle_panel::set_value(const bool selected)
{
	if(selected == get_value()) {
		return;
	}

	if(selected) {
		set_state(static_cast<tstate>(state_ + ENABLED_SELECTED));
	} else {
		set_state(static_cast<tstate>(state_ - ENABLED_SELECTED));
	}
}

void ttoggle_panel::set_retval(const int retval)
{
	retval_ = retval;
}

void ttoggle_panel::set_state(const tstate state)
{
	if(state == state_) {
		return;
	}

	state_ = state;
	set_dirty(true);

	boost::intrusive_ptr<const ttoggle_panel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const ttoggle_panel_definition::tresolution>(config());
	assert(conf);
}

bool ttoggle_panel::exist_anim() const
{
	return tcontrol::exist_anim() || (state_ >= ENABLED_SELECTED && canvas(COUNT).exist_anim());
}

void ttoggle_panel::impl_draw_foreground(surface& frame_buffer)
{
	if (state_ >= ENABLED_SELECTED) {
		std::vector<int> anims;
		canvas(COUNT).blit(frame_buffer, get_rect(), get_dirty(), anims, anims);
	}
}

void ttoggle_panel::impl_draw_foreground(surface& frame_buffer, int x_offset, int y_offset)
{
	if (state_ >= ENABLED_SELECTED) {
		std::vector<int> anims;
		canvas(COUNT).blit(frame_buffer, get_rect(), get_dirty(), anims, anims);
	}
}

const std::string& ttoggle_panel::get_control_type() const
{
	static const std::string type = "toggle_panel";
	return type;
}

void ttoggle_panel::signal_handler_mouse_enter(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!get_active()) {
		handled = true;
		return;
	}
	if(get_value()) {
		set_state(FOCUSSED_SELECTED);
	} else {
		set_state(FOCUSSED);
	}
	handled = true;
}

void ttoggle_panel::signal_handler_mouse_leave(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!get_active()) {
		handled = true;
		return;
	}
	if(get_value()) {
		set_state(ENABLED_SELECTED);
	} else {
		set_state(ENABLED);
	}
	handled = true;
}

void ttoggle_panel::signal_handler_pre_left_button_click(
		const event::tevent event)
{
	DBG_GUI_E << get_control_type() << "[" << id() << "]: " << event << ".\n";

	set_value(true);
	if(callback_state_change_) {
		callback_state_change_(this);
	}
}

void ttoggle_panel::signal_handler_left_button_click(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	sound::play_UI_sound(settings::sound_toggle_panel_click);

	if(get_value()) {
		set_state(ENABLED);
	} else {
		set_state(ENABLED_SELECTED);
	}

	if(callback_state_change_) {
		callback_state_change_(this);
	}
	handled = true;
}

void ttoggle_panel::signal_handler_left_button_double_click(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	if (retval_) {
		twindow* window = get_window();
		assert(window);

		window->set_retval(retval_);
	}

	if (callback_mouse_left_double_click_) {
		callback_mouse_left_double_click_(this);
	}
	handled = true;
}

} // namespace gui2

