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

#include "gui/dialogs/mode_setting.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"
#include "mkwin_controller.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/report.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/edit_box.hpp"
#include "gui/dialogs/message.hpp"

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

REGISTER_DIALOG(mode_setting)

tmode_setting::tmode_setting(mkwin_controller& controller, display& disp, unit_map& units)
	: tmode_navigate(controller, disp, *this, false)
	, units_(units)
	, ok_(NULL)
{
}

void tmode_setting::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* label = find_widget<tlabel>(&window, "title", false, true);

	ok_ = find_widget<tbutton>(&window, "ok", false, true);

	tmode_navigate::pre_show(window);

	// tlistbox& list = find_widget<tlistbox>(&window, "change_list", false);
	// list.set_callback_value_change(dialog_callback<tmode_setting, &tmode_setting::item_selected>);
	fill_change_list(window);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "delete_mode", false)
			, boost::bind(
				&tmode_setting::delete_mode
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "modify_mode", false)
			, boost::bind(
				&tmode_setting::modify_mode
				, this
				, boost::ref(window)));
}

void tmode_setting::post_show(twindow& window)
{
}


void tmode_setting::fill_change_list(twindow& window)
{
	std::stringstream ss;
	tlistbox& list = find_widget<tlistbox>(&window, "change_list", false);

	list.clear();
	const tmode& mode = controller_.mode(current_tab_);
	for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		unit* u = dynamic_cast<unit2*>(&*it);
		tadjust adjust = u->adjust(mode, tadjust::CHANGE);
		
		if (!adjust.valid()) {
			continue;
		}

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		ss.str("");
		ss << (list.get_item_count() + 1);
		list_item["label"] = ss.str();
		list_item_item.insert(std::make_pair("number", list_item));

		ss.str("");
		ss << u->cell().id;
		list_item["label"] = ss.str();
		list_item_item.insert(std::make_pair("change_id", list_item));

		list.add_row(list_item_item);
	}
	list.invalidate_layout(true);
}

int tmode_setting::calculate_change_count(int at) const
{
	int result = 0;
	const tmode& mode = controller_.mode(at);
	for (unit_map::const_iterator it = units_.begin(); it != units_.end(); ++ it) {
		unit* u = dynamic_cast<unit2*>(&*it);
		tadjust adjust = u->adjust(mode, tadjust::CHANGE);
		
		if (!adjust.valid()) {
			continue;
		}

		result ++;
	}
	return result;
}

void tmode_setting::toggle_tabbar(twidget* widget)
{
	tmode_navigate::toggle_tabbar(widget);

	twindow* window = widget->get_window();
	fill_change_list(*window);
}

std::string tmode_setting::form_tab_label(int at) const
{
	const tmode& mode = controller_.mode(at);

	std::stringstream ss;
	ss << mode.id;

	int count = calculate_change_count(at);
	if (count) {
		ss << "(";
		ss << tintegrate::generate_format(count, "green");
		ss << ")";
	}
	ss << "\n";
	ss << mode.res.width << "x" << mode.res.height;

	return ss.str();
}

void tmode_setting::item_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.get_selected_row();
}

void tmode_setting::delete_mode(twindow& window)
{
}

void tmode_setting::modify_mode(twindow& window)
{
}

}
