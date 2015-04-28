/* $Id: image.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/auxiliary/window_builder/image.hpp"

#include "config.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/widgets/image.hpp"

namespace gui2 {

namespace implementation {

tbuilder_image::tbuilder_image(const config& cfg)
	: tbuilder_control(cfg)
	, width_(cfg["width"])
	, height_(cfg["height"])
{
}

twidget* tbuilder_image::build() const
{
	timage* widget = new timage();

	init_control(widget);

	const game_logic::map_formula_callable& size = get_screen_size_variables();

	const unsigned width = width_(size);
	const unsigned height = height_(size);

	if (width || height) {
		widget->set_best_size(tpoint(width, height));
	}

	DBG_GUI_G << "Window builder: placed image '"
			<< id << "' with definition '"
			<< definition << "'.\n";

	return widget;
}

} // namespace implementation

} // namespace gui2

/*WIKI_MACRO
 * @begin{macro}{image_description}
 * An image shows a static image.
 * @end{macro}
 */

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_image
 *
 * == Image ==
 *
 * @macro = image_description
 *
 * An image has no extra fields.
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="image"}{min=0}{max=-1}{super="generic/widget_instance"}
 * @end{tag}{name="image"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

