/* $Id: scrollbar_panel.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2009 - 2012 by Mark de Wever <koraq@xs4all.nl>
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

#include "gui/widgets/scrollbar_panel.hpp"

#include "gui/auxiliary/widget_definition/scrollbar_panel.hpp"
#include "gui/auxiliary/window_builder/scrollbar_panel.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scrollbar.hpp"

#include <boost/bind.hpp>

namespace gui2 {

REGISTER_WIDGET(scrollbar_panel)

void tscrollbar_panel::place_content_grid(const tpoint& content_origin, const tpoint& desire_size, const tpoint& origin)
{
	const tpoint actual_size = content_grid_->get_best_size();
	calculate_scrollbar(actual_size, desire_size);

	const tpoint size(std::max(actual_size.x, desire_size.x), std::max(actual_size.y, desire_size.y));
	content_grid_->place(origin, size);
}

const std::string& tscrollbar_panel::get_control_type() const
{
    static const std::string type = "scrollbar_panel";
    return type;
}

} // namespace gui2

