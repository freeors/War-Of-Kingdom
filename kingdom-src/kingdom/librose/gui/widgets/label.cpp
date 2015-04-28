/* $Id: label.cpp 54038 2012-04-30 19:37:24Z mordante $ */
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

#include "gui/widgets/label.hpp"

#include "gui/auxiliary/widget_definition/label.hpp"
#include "gui/auxiliary/window_builder/label.hpp"
#include "gui/widgets/settings.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(label)

tpoint tlabel::request_reduce_width(const unsigned maximum_width)
{
	unsigned w = maximum_width;

	ttext_maximum_width_lock lock(*this, maximum_width);
	tpoint size = calculate_best_size();

	insert_reduce_widget(this, size);
	return size;
}

bool tlabel::can_wrap() const
{
	return can_wrap_ || characters_per_line_ != 0;
}

unsigned tlabel::get_characters_per_line() const
{
	return characters_per_line_;
}

void tlabel::set_characters_per_line(const unsigned characters_per_line)
{
	characters_per_line_ = characters_per_line;
}

void tlabel::set_state(const tstate state)
{
	if(state != state_) {
		state_ = state;
		set_dirty(true);
	}
}

const std::string& tlabel::get_control_type() const
{
	static const std::string type = "label";
	return type;
}

} // namespace gui2

