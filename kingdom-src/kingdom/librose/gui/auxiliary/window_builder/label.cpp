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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/auxiliary/window_builder/label.hpp"

#include "config.hpp"
#include "gui/auxiliary/log.hpp"
#include "gui/widgets/label.hpp"

namespace gui2 {

namespace implementation {

tbuilder_label::tbuilder_label(const config& cfg)
	: tbuilder_control(cfg)
	, wrap(cfg["wrap"].to_bool())
	, characters_per_line(cfg["characters_per_line"])
	, text_alignment(decode_text_alignment(cfg["text_alignment"]))
	, width_(cfg["width"])
	, height_(cfg["height"])
{
}

twidget* tbuilder_label::build() const
{
	tlabel* label = new tlabel();

	init_control(label);

	int text_maximum_width = fix_rect.w;
	if (width_.has_formula() || height_.has_formula()) {
		const game_logic::map_formula_callable& size = get_screen_size_variables();

		unsigned width = width_(size);
		unsigned height = height_(size);

		text_maximum_width = width;
		if (width || height) {
			label->set_best_size(tpoint(width, height));
		}
	}
	if (text_maximum_width) {
		label->set_text_maximum_width(text_maximum_width);
	}
	
	label->set_can_wrap(wrap);
	label->set_characters_per_line(characters_per_line);
	label->set_text_alignment(text_alignment);

	DBG_GUI_G << "Window builder: placed label '"
			<< id << "' with definition '"
			<< definition << "'.\n";

	return label;
}

} // namespace implementation

} // namespace gui2

/*WIKI_MACRO
 * @begin{macro}{label_description}
 *
 *        A label displays a text, the text can be wrapped but no scrollbars
 *        are provided.
 * @end{macro}
 */

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_label
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="label"}{min=0}{max=-1}{super="generic/widget_instance"}
 * == Label ==
 *
 * @macro = label_description
 *
 * List with the label specific variables:
 * @begin{table}{config}
 *     wrap & bool & false &      Is wrapping enabled for the label. $
 *     characters_per_line & unsigned & 0 &
 *                                Sets the maximum number of characters per
 *                                line. The amount is an approximate since the
 *                                width of a character differs. E.g. iii is
 *                                smaller than MMM. When the value is non-zero
 *                                it also implies can_wrap is true.
 *                                When having long strings wrapping them can
 *                                increase readability, often 66 characters per
 *                                line is considered the optimum for a one
 *                                column text.
 *     text_alignment & h_align & "left" &
 *                                How is the text aligned in the label. $
 * @end{table}
 * @end{tag}{name="label"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

