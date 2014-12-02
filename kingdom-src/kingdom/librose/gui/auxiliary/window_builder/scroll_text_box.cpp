/* $Id: text_box.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#include "gui/auxiliary/window_builder/scroll_text_box.hpp"

#include "config.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/auxiliary/widget_definition/scroll_text_box.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/scroll_text_box.hpp"

namespace gui2 {

namespace implementation {

tbuilder_scroll_text_box::tbuilder_scroll_text_box(const config& cfg)
	: tbuilder_control(cfg)
	, vertical_scrollbar_mode(
			get_scrollbar_mode(cfg["vertical_scrollbar_mode"]))
	, horizontal_scrollbar_mode(
			get_scrollbar_mode(cfg["horizontal_scrollbar_mode"]))
	, width(cfg["width"])
	, height(cfg["height"])
{
}

twidget* tbuilder_scroll_text_box::build() const
{
	tscroll_text_box* widget = new tscroll_text_box();

	init_control(widget);

	widget->set_vertical_scrollbar_mode(vertical_scrollbar_mode);
	widget->set_horizontal_scrollbar_mode(horizontal_scrollbar_mode);

	boost::intrusive_ptr<const tscroll_text_box_definition::tresolution> conf =
			boost::dynamic_pointer_cast
				<const tscroll_text_box_definition::tresolution>(widget->config());
	assert(conf);

	widget->init_grid(conf->grid);
	widget->finalize_setup();

	// A textbox doesn't have a label but a text
	ttext_box* tb = dynamic_cast<ttext_box*>(widget->content_grid()->find("_text_box", false));
	if (width.has_formula()) {
		const game_logic::map_formula_callable& size = get_screen_size_variables();
		const unsigned w = width(size);
		if (w) {
			tb->set_text_maximum_width(w - tb->config()->text_extra_width);
			unsigned h = 0;
			if (height.has_formula()) {
				h = height(size);
			}
			widget->set_best_size(tpoint(w, h));
		}
	}
	tb->set_value(label);

	DBG_GUI_G << "Window builder: placed text box '"
			<< id << "' with definition '"
			<< definition << "'.\n";

	return widget;
}

} // namespace implementation

} // namespace gui2

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_text_box
 *
 * == Text box ==
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="text_box"}{min="0"}{max="-1"}{super="generic/widget_instance"}
 * @begin{table}{config}
 *     label & t_string & "" &          The initial text of the text box. $
 *     history & string & "" &         The name of the history for the text
 *                                     box.
 *                                     A history saves the data entered in a
 *                                     text box between the games. With the up
 *                                     and down arrow it can be accessed. To
 *                                     create a new history item just add a
 *                                     new unique name for this field and the
 *                                     engine will handle the rest. $
 * @end{table}
 * @end{tag}{name="text_box"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

