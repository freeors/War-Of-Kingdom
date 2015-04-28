/* $Id: mp_method_selection.cpp 48440 2011-02-07 20:57:31Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/mp_method_selection.hpp"

#include "game_preferences.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "multiplayer.hpp"
#include "gettext.hpp"

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_mp_method_selection
 *
 * == MP method selection ==
 *
 * This shows the dialog to select the kind of MP game the user wants to play.
 *
 * @begin{table}{dialog_widgets}
 *
 * user_name & & text_box & m &
 *         This text contains the name the user on the MP server. This widget
 *         will get a fixed maximum length by the engine. $
 *
 * method_list & & listbox & m &
 *         The list with possible game methods. $
 *
 * @end{table}
 */

REGISTER_DIALOG(mp_method_selection)

struct titem 
{
	titem(const std::string& icon, const std::string& text)
		: icon(icon)
		, text(text)
	{}

	std::string icon;
	std::string text;
};

void tmp_method_selection::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::stringstream strstr;
	user_name_ = preferences::login();
	tcontrol* user_widget = find_widget<tcontrol>(
			&window, "user_name", false, true);
	user_widget->set_label(user_name_);

	tlistbox& list = find_widget<tlistbox>(&window, "method_list", false);
	window.keyboard_capture(&list);

	std::vector<titem> items;
	items.push_back(titem("icons/icon-player.png", _("Attack Player")));
	items.push_back(titem("icons/icon-subcontinent.png", _("Crusage for subcontinent")));
	items.push_back(titem("icons/icon-server.png", _("Enter multiplayer lobby")));
#ifdef _WIN32
	items.push_back(titem("icons/icon-hostgame.png", _("Host Networked Game")));
#endif

	for (std::vector<titem>::const_iterator it = items.begin(); it != items.end(); ++ it) {
		const titem& item = *it;

		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		strstr.str("");
		strstr << item.icon;
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = item.text;
		list_item_item.insert(std::make_pair("text", list_item));

		list.add_row(list_item_item);
	}
}

void tmp_method_selection::post_show(twindow& window)
{
	if(get_retval() == twindow::OK) {
		tlistbox& list = find_widget<tlistbox>(&window, "method_list", false);

		choice_ = list.get_selected_row();
	}
}

} // namespace gui2
