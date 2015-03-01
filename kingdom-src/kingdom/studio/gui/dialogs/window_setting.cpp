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

#include "gui/dialogs/window_setting.hpp"

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
#include "gui/widgets/scrollbar_panel.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"
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

REGISTER_DIALOG(window_setting)

twindow_setting::twindow_setting(display& disp, const unit& u, const std::vector<std::string>& textdomains)
	: tsetting_dialog(u.cell())
	, disp_(disp)
	, u_(u)
	, textdomains_(textdomains)
{
}

void twindow_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream ss;
	const std::pair<std::string, gui2::tcontrol_definition_ptr>& widget = u_.widget();

	ss.str("");
	ss << "Window setting"; 
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	label->set_label(ss.str());

	ttext_box* text_box = find_widget<ttext_box>(&window, "id", false, true);
	text_box->set_label(cell_.id);

	text_box = find_widget<ttext_box>(&window, "description", false, true);
	text_box->set_value(cell_.window.description);

	set_textdomain_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "textdomain", false)
			, boost::bind(
				&twindow_setting::set_textdomain
				, this
				, boost::ref(window)));

	set_definition_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "set_definition", false)
			, boost::bind(
				&twindow_setting::set_definition
				, this
				, boost::ref(window)));

	ttoggle_button* toggle = find_widget<ttoggle_button>(&window, "click_dismiss1", false, true);
	toggle->set_value(cell_.window.click_dismiss);

	toggle = find_widget<ttoggle_button>(&window, "automatic_placement", false, true);
	toggle->set_value(cell_.window.automatic_placement);
	toggle->set_callback_state_change(boost::bind(&twindow_setting::automatic_placement_toggled, this, _1));

	page_panel_ = find_widget<tscrollbar_panel>(&window, "layout", false, true);
	if (cell_.window.automatic_placement) {
		swap_page(window, AUTOMATIC_PAGE, false);
	} else {
		swap_page(window, MANUAL_PAGE, false);
	}

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&twindow_setting::save
				, this
				, boost::ref(window)
				, _3, _4));
}

void twindow_setting::set_definition(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	const gui2::tgui_definition::tcontrol_definition_map& controls = gui2::current_gui->second.control_definition;
	const std::map<std::string, gui2::tcontrol_definition_ptr>& windows = controls.find("window")->second;

	int index = 0;
	int def = 0;
	for (std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = windows.begin(); it != windows.end(); ++ it) {
		ss.str("");
		ss << tintegrate::generate_format(it->first, "yellow") << "(" << it->second->description << ")";
		if (it->first == cell_.window.definition) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	std::map<std::string, gui2::tcontrol_definition_ptr>::const_iterator it = windows.begin();
	std::advance(it, dlg.selected_val());
	cell_.window.definition = it->first;

	set_definition_label(window);
}

void twindow_setting::set_definition_label(twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "definition", false, true);
	label->set_label(cell_.window.definition);
}

void twindow_setting::set_horizontal_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tlayout>::const_iterator it = horizontal_layout.begin(); it != horizontal_layout.end(); ++ it) {
		if (it->second.val == gui2::tgrid::HORIZONTAL_GROW_SEND_TO_CLIENT) {
			continue;
		}
		ss.str("");
		ss << tintegrate::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.window.horizontal_placement);
	dlg.show(disp_.video());

	cell_.window.horizontal_placement = dlg.selected_val();

	set_horizontal_layout_label(window);
}

void twindow_setting::set_vertical_layout(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	for (std::map<int, tlayout>::const_iterator it = vertical_layout.begin(); it != vertical_layout.end(); ++ it) {
		if (it->second.val == gui2::tgrid::VERTICAL_GROW_SEND_TO_CLIENT) {
			continue;
		}
		ss.str("");
		ss << tintegrate::generate_img(it->second.icon + "~SCALE(24, 24)") << it->second.description;
		items.push_back(tval_str(it->first, ss.str()));
	}

	gui2::tcombo_box dlg(items, cell_.window.vertical_placement);
	dlg.show(disp_.video());

	cell_.window.vertical_placement = dlg.selected_val();

	set_vertical_layout_label(window);
}

void twindow_setting::set_horizontal_layout_label(twindow& window)
{
	std::stringstream ss;

	ss << horizontal_layout.find(cell_.window.horizontal_placement)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_horizontal_layout", true, true);
	label->set_label(ss.str());
}

void twindow_setting::set_vertical_layout_label(twindow& window)
{
	std::stringstream ss;

	ss << vertical_layout.find(cell_.window.vertical_placement)->second.description;
	tlabel* label = find_widget<tlabel>(&window, "_vertical_layout", true, true);
	label->set_label(ss.str());
}

void twindow_setting::automatic_placement_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	twindow* window = toggle->get_window();
	if (toggle->get_value()) {
		swap_page(*window, AUTOMATIC_PAGE, true);
	} else {
		swap_page(*window, MANUAL_PAGE, true);
	}
}

void twindow_setting::set_textdomain(twindow& window)
{
	std::stringstream ss;
	std::vector<tval_str> items;

	int index = 0;
	int def = 0;
	for (std::vector<std::string>::const_iterator it = textdomains_.begin(); it != textdomains_.end(); ++ it) {
		ss.str("");
		ss << *it;
		if (*it == cell_.window.textdomain) {
			def = index;
		}
		items.push_back(tval_str(index ++, ss.str()));
	}

	gui2::tcombo_box dlg(items, def);
	dlg.show(disp_.video());

	index = dlg.selected_val();
	cell_.window.textdomain = textdomains_[index];

	set_textdomain_label(window);
}

void twindow_setting::set_textdomain_label(twindow& window)
{
	std::stringstream ss;
	ss << cell_.window.textdomain;

	tbutton* button = find_widget<tbutton>(&window, "textdomain", false, true);
	button->set_label(ss.str());
}

void twindow_setting::save(twindow& window, bool& handled, bool& halt)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "id", false, true);
	cell_.id = text_box->label();
	if (!utils::isvalid_id(cell_.id, false, min_id_len, max_id_len)) {
		handled = true;
		halt = true;
		show_id_error(disp_, "id", utils::errstr);
		return ;
	}
	text_box = find_widget<ttext_box>(&window, "description", false, true);
	cell_.window.description = text_box->get_value();
	if (cell_.window.description.empty()) {
		handled = true;
		halt = true;
		show_id_error(disp_, "description", _("Can not empty!"));
		return ;
	}

	cell_.window.click_dismiss = find_widget<ttoggle_button>(&window, "click_dismiss1", false, true)->get_value();
	cell_.window.automatic_placement = find_widget<ttoggle_button>(&window, "automatic_placement", false, true)->get_value();
	if (!cell_.window.automatic_placement) {
		text_box = find_widget<ttext_box>(&window, "x", false, true);
		cell_.window.x = text_box->get_value();
		text_box = find_widget<ttext_box>(&window, "y", false, true);
		cell_.window.y = text_box->get_value();
		text_box = find_widget<ttext_box>(&window, "width", false, true);
		cell_.window.width = text_box->get_value();
		text_box = find_widget<ttext_box>(&window, "height", false, true);
		cell_.window.height = text_box->get_value();
	}

	window.set_retval(twindow::OK);
}

void twindow_setting::fill_automatic_page(twindow& window)
{
	// horizontal layout
	set_horizontal_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_horizontal_layout", false)
			, boost::bind(
				&twindow_setting::set_horizontal_layout
				, this
				, boost::ref(window)));

	// vertical layout
	set_vertical_layout_label(window);
	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "_set_vertical_layout", false)
			, boost::bind(
				&twindow_setting::set_vertical_layout
				, this
				, boost::ref(window)));
}

void twindow_setting::fill_manual_page(twindow& window)
{
	ttext_box* text_box = find_widget<ttext_box>(&window, "x", false, true);
	text_box->set_value(cell_.window.x);

	text_box = find_widget<ttext_box>(&window, "y", false, true);
	text_box->set_value(cell_.window.y);

	text_box = find_widget<ttext_box>(&window, "width", false, true);
	text_box->set_value(cell_.window.width);

	text_box = find_widget<ttext_box>(&window, "height", false, true);
	text_box->set_value(cell_.window.height);
}

void twindow_setting::swap_page(twindow& window, int page, bool swap)
{
	if (!page_panel_) {
		return;
	}
	int orignal = page_panel_->current_page();
	if (orignal == page) {
		// desired page is the displaying page, do nothing.
		return;
	}

	if (orignal == AUTOMATIC_PAGE) {

	} else if (orignal == MANUAL_PAGE) {

	}

	page_panel_->swap_uh(window, page);
	if (swap) {
		page_panel_->swap_bh(window);
	}

	if (page == AUTOMATIC_PAGE) {
		fill_automatic_page(window);

	} else if (page == MANUAL_PAGE) {
		fill_manual_page(window);

	}
}

}
