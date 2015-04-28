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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/auxiliary/window_builder/text_box2.hpp"

#include "config.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/window_builder/helper.hpp"
#include "gui/auxiliary/widget_definition/text_box2.hpp"
#include "gui/widgets/text_box2.hpp"

namespace gui2 {

namespace implementation {

tbuilder_text_box2::tbuilder_text_box2(const config& cfg)
	: tbuilder_control(cfg)
	, width(cfg["width"])
	, height(cfg["height"])
	, text_box_definition(cfg["text_box"])
{
	if (text_box_definition.empty()) {
		text_box_definition = "default";
	}
}

twidget* tbuilder_text_box2::build() const
{

	ttext_box2* widget = new ttext_box2();

	init_control(widget);

	// widget->set_best_size(width, height);

	boost::intrusive_ptr<const ttext_box2_definition::tresolution> conf =
			boost::dynamic_pointer_cast
				<const ttext_box2_definition::tresolution>(widget->config());

	// update definition of button to itself.
	tbuilder_grid* grid_ptr = conf->grid.get();
	if (reinterpret_cast<tbuilder_control*>(grid_ptr->widgets[0].get())->definition != text_box_definition) {
		tbuilder_grid grid = *grid_ptr;
		tbuilder_control* text_box = reinterpret_cast<tbuilder_control*>(grid.widgets[0].get());
		text_box->definition = text_box_definition;
	}

	widget->init_grid(conf->grid);
	// widget->finalize_setup();

	// A textbox doesn't have a label but a text
	ttext_box* tb = dynamic_cast<ttext_box*>(widget->grid().find("_text_box", false));
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

