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

#include "gui/dialogs/linked_group.hpp"

#include "display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/dialogs/combo_box.hpp"
#include "gui/dialogs/message.hpp"

// std::tolower
#include <cctype>

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

REGISTER_DIALOG(linked_group2)

tlinked_group2::tlinked_group2(display& disp, const std::vector<tlinked_group>& linked_groups)
	: disp_(disp)
	, orignal_(linked_groups)
	, linked_groups_(linked_groups)
{
}

void tlinked_group2::pre_show(CVideo& /*video*/, twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	// window.keyboard_capture(&list);

	std::map<std::string, string_map> data;

	reload_linked_group_table(window, 0);
	list.set_callback_value_change(dialog_callback<tlinked_group2, &tlinked_group2::item_selected>);

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "append", false)
			, boost::bind(
				&tlinked_group2::append
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "modify", false)
			, boost::bind(
				&tlinked_group2::modify
				, this
				, boost::ref(window)));

	connect_signal_mouse_left_click(
			  find_widget<tbutton>(&window, "delete", false)
			, boost::bind(
				&tlinked_group2::erase
				, this
				, boost::ref(window)));

}

void tlinked_group2::reload_linked_group_table(twindow& window, int cursel)
{
	tlistbox* list = find_widget<tlistbox>(&window, "default", false, true);
	list->clear();

	int index = 0;
	for (std::vector<tlinked_group>::const_iterator it = linked_groups_.begin(); it != linked_groups_.end(); ++ it) {
		const tlinked_group& linked = *it;

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = str_cast(index ++);
		list_item_item.insert(std::make_pair("number", list_item));

		list_item["label"] = linked.id;
		list_item_item.insert(std::make_pair("id", list_item));

		list_item["label"] = linked.fixed_width? "yes": "-";
		list_item_item.insert(std::make_pair("width", list_item));

		list_item["label"] = linked.fixed_height? "yes": "-";
		list_item_item.insert(std::make_pair("height", list_item));

		list_item["label"] = "Window";
		list_item_item.insert(std::make_pair("ownership", list_item));

		list->add_row(list_item_item);
	}
	if (cursel >= (int)linked_groups_.size()) {
		cursel = (int)linked_groups_.size() - 1;
	}
	if (!linked_groups_.empty()) {
		list->select_row(cursel);
	}
	window.invalidate_layout();

	map_linked_group_to(window, cursel);
}

std::string tlinked_group2::get_id(twindow& window, int exclude)
{
	std::string id = find_widget<ttext_box>(&window, "_id", false, true)->label();
	if (!utils::isvalid_id(id, true, min_id_len, max_id_len)) {
		gui2::show_message(disp_.video(), "", utils::errstr);
		return null_str;
	}
	utils::transform_tolower(id);

	int index = 0;
	for (std::vector<tlinked_group>::const_iterator it = linked_groups_.begin(); it != linked_groups_.end(); ++ it, index ++) {
		if (index == exclude) {
			continue;
		}
		if (id == linked_groups_[index].id) {
			std::stringstream err;
			err << dgettext("wesnoth-lib", "String is using.");
			gui2::show_message(disp_.video(), "", err.str());
			return null_str;
		}
	}
	return id;
}

bool tlinked_group2::verify_other_param(bool width, bool height)
{
	if (!width && !height) {
		std::stringstream err;
		err << dgettext("wesnoth-lib", "Both width and height must not be all No!");
		gui2::show_message(disp_.video(), "", err.str());
		return false;
	}
	return true;
}

void tlinked_group2::map_linked_group_to(twindow& window, int index)
{
	static const tlinked_group null_linked_group;

	bool valid = !linked_groups_.empty();
	const tlinked_group& linked = valid? linked_groups_[index]: null_linked_group;
	
	ttext_box* id = find_widget<ttext_box>(&window, "_id", false, true);
	id->set_label(linked.id);

	ttoggle_button* width = find_widget<ttoggle_button>(&window, "_width", false, true);
	width->set_value(linked.fixed_width);

	ttoggle_button* height = find_widget<ttoggle_button>(&window, "_height", false, true);
	height->set_value(linked.fixed_height);

	find_widget<tcontrol>(&window, "modify", false, true)->set_active(valid);
	find_widget<tcontrol>(&window, "delete", false, true)->set_active(valid);
}

void tlinked_group2::append(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	std::string id = get_id(window, -1);
	if (id.empty()) {
		return;
	}
	bool width = find_widget<ttoggle_button>(&window, "_width", false, true)->get_value();
	bool height = find_widget<ttoggle_button>(&window, "_height", false, true)->get_value();
	if (!verify_other_param(width, height)) {
		return;
	}
	linked_groups_.push_back(tlinked_group(id, width, height));

	reload_linked_group_table(window, linked_groups_.size() - 1);
}

void tlinked_group2::modify(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.get_selected_row();
	tlinked_group& linked = linked_groups_[row];

	ttext_box* text_box = find_widget<ttext_box>(&window, "_id", false, true);
	std::string id = get_id(window, row);
	if (id.empty()) {
		return;
	}
	bool width = find_widget<ttoggle_button>(&window, "_width", false, true)->get_value();
	bool height = find_widget<ttoggle_button>(&window, "_height", false, true)->get_value();
	if (!verify_other_param(width, height)) {
		return;
	}
	if (id != linked.id || width != linked.fixed_width || height != linked.fixed_height) {
		linked_groups_[row] = tlinked_group(id, width, height);
		reload_linked_group_table(window, row);
	}
}

void tlinked_group2::erase(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.get_selected_row();

	std::vector<tlinked_group>::iterator it = linked_groups_.begin();
	std::advance(it, row);
	linked_groups_.erase(it);
	reload_linked_group_table(window, row);
}

void tlinked_group2::item_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "default", false);
	int row = list.get_selected_row();
	map_linked_group_to(window, row);
}

}
