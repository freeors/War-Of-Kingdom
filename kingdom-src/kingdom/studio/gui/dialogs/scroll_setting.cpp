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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/scroll_setting.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/scroll_text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "unit.hpp"

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

REGISTER_DIALOG(scroll_setting)

tscroll_setting::tscroll_setting(display& disp, const unit& u)
	: tsetting_dialog(u.cell())
	, disp_(disp)
	, u_(u)
{
}

void tscroll_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << _("Special Setting"); 
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	ttext_box* text_box;
	if (!u_.is_tree_view()) {
		text_box = find_widget<ttext_box>(&window, "indention_step_size", false, true);
		text_box->set_visible(twidget::INVISIBLE);
		text_box = find_widget<ttext_box>(&window, "node_id", false, true);
		text_box->set_visible(twidget::INVISIBLE);

		find_widget<tlabel>(&window, "indention_step_size_label", false, true)->set_visible(twidget::INVISIBLE);
		find_widget<tlabel>(&window, "node_id_label", false, true)->set_visible(twidget::INVISIBLE);
	} else {
		text_box = find_widget<ttext_box>(&window, "indention_step_size", false, true);
		text_box->set_label(str_cast(cell_.widget.tree_view.indention_step_size));
		text_box = find_widget<ttext_box>(&window, "node_id", false, true);
		text_box->set_label(cell_.widget.tree_view.node_id);
	}

	// horizontal mode
	set_horizontal_mode_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_horizontal_mode", false)
			, boost::bind(
				&tscroll_setting::set_horizontal_mode
				, this
				, boost::ref(window)));

	// vertical layout
	set_vertical_mode_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_vertical_mode", false)
			, boost::bind(
				&tscroll_setting::set_vertical_mode
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&tscroll_setting::save
				, this
				, boost::ref(window)
				, _3, _4));
}

void tscroll_setting::set_horizontal_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tscroll_mode>::const_iterator it = horizontal_mode.begin(); it != horizontal_mode.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.widget.horizontal_mode);
	dlg.show(disp_.video());

	cell_.widget.horizontal_mode = (tscrollbar_container::tscrollbar_mode)dlg.selected_val();

	set_horizontal_mode_label(window);
}

void tscroll_setting::set_horizontal_mode_label(twindow& window)
{
	std::stringstream ss;

	ss << horizontal_mode.find(cell_.widget.horizontal_mode)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_mode", false, true);
	label->set_label(ss.str());
}

void tscroll_setting::set_vertical_mode(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tscroll_mode>::const_iterator it = vertical_mode.begin(); it != vertical_mode.end(); ++ it) {
		ss.str("");
		ss << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.widget.vertical_mode);
	dlg.show(disp_.video());

	cell_.widget.vertical_mode = (tscrollbar_container::tscrollbar_mode)dlg.selected_val();

	set_vertical_mode_label(window);
}

void tscroll_setting::set_vertical_mode_label(twindow& window)
{
	std::stringstream ss;

	ss << vertical_mode.find(cell_.widget.vertical_mode)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_mode", false, true);
	label->set_label(ss.str());
}

void tscroll_setting::save(twindow& window, bool& handled, bool& halt)
{
	if (u_.is_tree_view()) {
		ttext_box* text_box = find_widget<ttext_box>(&window, "indention_step_size", false, true);
		cell_.widget.tree_view.indention_step_size = lexical_cast_default<int>(text_box->label());
		text_box = find_widget<ttext_box>(&window, "node_id", false, true);
		cell_.widget.tree_view.node_id = text_box->get_value();
		if (cell_.widget.tree_view.node_id.empty()) {
			handled = true;
			halt = true;
			show_id_error(disp_, "node's id", _("Can not empty!"));
			return;
		}
	}

	window.set_retval(twindow::OK);
}

}
