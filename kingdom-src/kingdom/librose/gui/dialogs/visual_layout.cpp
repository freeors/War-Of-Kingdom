/* $Id: campaign_difficulty.cpp 49602 2011-05-22 17:56:13Z mordante $ */
/*
   Copyright (C) 2010 - 2011 by Ignacio Riquelme Morelle <shadowm2006@gmail.com>
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

#include "gui/dialogs/visual_layout.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/report.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_difficulty
 *
 * == Campaign difficulty ==
 *
 * The campaign mode difficulty menu.
 *
 * @begin{table}{dialog_widgets}
 *
 * title & & label & m &
 *         Dialog title label. $
 *
 * message & & scroll_label & o &
 *         Text label displaying a description or instructions. $
 *
 * listbox & & listbox & m &
 *         Listbox displaying user choices, defined by WML for each campaign. $
 *
 * -icon & & control & m &
 *         Widget which shows a listbox item icon, first item markup column. $
 *
 * -label & & control & m &
 *         Widget which shows a listbox item label, second item markup column. $
 *
 * -description & & control & m &
 *         Widget which shows a listbox item description, third item markup column. $
 *
 * @end{table}
 */

REGISTER_DIALOG(visual_layout)

tvisual_layout::tvisual_layout(display& disp, const twindow& target, const std::string& reason)
	: disp_(disp)
	, target_(target)
	, reason_(reason)
{
}

void tvisual_layout::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(target_.id());

	label = find_widget<tlabel>(&window, "reason", false, true);
	label->set_label(reason_);

	layout_report_ = find_widget<treport>(&window, "layout_report", false, true);
	layout_report_->multiline_init(true, "text_cell", 0, 0, 2);

	goto_parent_ = find_widget<tbutton>(&window, "goto_parent", false, true);
	current_label_ = find_widget<tlabel>(&window, "current_label", false, true);
	into_child_ = find_widget<tbutton>(&window, "into_child", false, true);

	connect_signal_mouse_left_click(
		  *goto_parent_
		, boost::bind(
			  &tvisual_layout::goto_parent
			, this
			, boost::ref(window)));

	connect_signal_mouse_left_click(
		  *into_child_
		, boost::bind(
			  &tvisual_layout::into_child
			, this
			, boost::ref(window)));

	const tgrid& grid = target_.grid();
	reload_layout_report(grid);
}

void tvisual_layout::reload_layout_report(const tgrid& grid)
{
	layout_report_->erase_children();

	layout_report_->multiline_set(twidget::npos, grid.cols_);
	for (unsigned row = 0; row < grid.rows_; ++row) {
		for (unsigned col = 0; col < grid.cols_; ++ col) {

			const tgrid::tchild& child = grid.child(row, col);

			tcontrol* widget = layout_report_->create_child(null_str, null_str, reinterpret_cast<void*>(child.widget_));
			widget->set_label(generate_widget_str(row, col, grid.col_width_[col], grid.row_height_[row], child));
			layout_report_->insert_child(*widget);
		}
	}
	layout_report_->select(0);
	layout_report_->replacement_children();

	toggle_report(layout_report_->get_widget(0));
	current_widget_ = grid.child(0, 0).widget_;
}

void tvisual_layout::toggle_report(twidget* widget)
{
	twindow* window = widget->get_window();

	current_widget_ = reinterpret_cast<twidget*>(widget->cookie());
	tdialog::toggle_report(widget);

	std::stringstream ss;
	std::string id;
	if (current_widget_->id().empty()) {
		id = "--";
	} else {
		id = current_widget_->id();
	}

	ss << _("Type") << "  " << current_widget_->get_control_type();
	ss << "    id  " << id;
	current_label_->set_label(ss.str());

	goto_parent_->set_active(!!calculate_goto_parent(*window, current_widget_));

	bool has_child = false;
	if (dynamic_cast<tgrid*>(current_widget_)) {
		has_child = true;
	} else if (dynamic_cast<tcontainer_*>(current_widget_)) {
		has_child = true;
	}
	into_child_->set_active(has_child);

}

const twidget* tvisual_layout::calculate_goto_parent(twindow& window, const twidget* widget) const
{
	const twidget* parent = widget->parent();
	parent = parent->parent();
	if (!parent) {
		return NULL;
	} else if (dynamic_cast<const tgrid*>(parent)) {
		return parent;
	}
	parent = parent->parent();
	return parent;
}

void tvisual_layout::goto_parent(twindow& window)
{
	const tgrid* grid = NULL;
	const twidget* parent = calculate_goto_parent(window, current_widget_);
	if (const tcontainer_* container = dynamic_cast<const tcontainer_*>(parent)) {
		grid = &container->grid();
	}
	if (!grid) {
		grid = dynamic_cast<const tgrid*>(parent);
	}
	reload_layout_report(*grid);
}

void tvisual_layout::into_child(twindow& window)
{
	const tgrid* grid = NULL;
	if (tcontainer_* container = dynamic_cast<tcontainer_*>(current_widget_)) {
		grid = &container->layout_grid();
	}
	if (!grid) {
		grid = dynamic_cast<tgrid*>(current_widget_);
	}
	reload_layout_report(*grid);
}

std::string tvisual_layout::generate_widget_str(int row, int col, int width, int height, const tgrid::tchild& child)
{
	const twidget& widget = *child.widget_;
	std::stringstream ss;

	ss << "(" << row << ", " << col << ")\n";
	if (!widget.id().empty()) {
		ss << tintegrate::generate_format(widget.id(), "green");
	} else {
		ss << tintegrate::generate_format("--", "green");
	}
	ss << "(" << widget.get_control_type() << ")";
	ss << "\n";
	ss << width << "x" << height;
	return ss.str();
}

}
