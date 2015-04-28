/* $Id: panel.cpp 54007 2012-04-28 19:16:10Z mordante $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/panel.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/panel.hpp"
#include "gui/auxiliary/window_builder/panel.hpp"
#include "gui/widgets/settings.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(panel)

tpanel::tpanel(const unsigned canvas_count) 
	: tcontainer_(canvas_count)
	, hole_left_(0)
	, hole_right_(0)
{}

SDL_Rect tpanel::get_client_rect() const
{
	boost::intrusive_ptr<const tpanel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tpanel_definition::tresolution>(config());
	assert(conf);

	SDL_Rect result = get_rect();
	result.x += conf->left_border;
	result.y += conf->top_border;
	result.w -= conf->left_border + conf->right_border;
	result.h -= conf->top_border + conf->bottom_border;

	return result;
}

void tpanel::set_hole_variable(int left, int right)
{
	if (left < 0 || right < 0) {
		return;
	}

	if (left != hole_left_ || right != hole_right_) {
		hole_left_ = left;
		hole_right_ = right;
		if (x_ >= 0) {
			update_canvas();
			set_dirty();
		}
	}
}

void tpanel::update_canvas()
{
	// Inherit.
	tcontrol::update_canvas();

	int left = hole_left_ - x_;
	int right = hole_right_ - x_;
	right -= 1;
	if (left < 0) {
		left = 0;
	}
	if (right < 0) {
		right = 0;
	}

	// set icon in canvases
	canvas(0).set_variable("hole_left", variant(left));
	canvas(0).set_variable("hole_right", variant(right));
}

bool tpanel::exist_anim()
{
	return tcontrol::exist_anim() || canvas(1).exist_anim();
}

void tpanel::impl_draw_background(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	// tpanel's get_state must be return 0.
	// look canvas(0) as standard canvas. animation is add to it.

	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);
}

void tpanel::impl_draw_foreground(
		  surface& frame_buffer
		, int x_offset
		, int y_offset)
{
	std::vector<int> anims;
	canvas(1).blit(
			  frame_buffer
			, calculate_blitting_rectangle(x_offset, y_offset), get_dirty(), anims, anims);
}

tpoint tpanel::border_space() const
{
	boost::intrusive_ptr<const tpanel_definition::tresolution> conf =
		boost::dynamic_pointer_cast<const tpanel_definition::tresolution>(config());
	assert(conf);

	return tpoint(conf->left_border + conf->right_border,
		conf->top_border + conf->bottom_border);
}

const std::string& tpanel::get_control_type() const
{
	static const std::string type = "panel";
	return type;
}

} // namespace gui2

