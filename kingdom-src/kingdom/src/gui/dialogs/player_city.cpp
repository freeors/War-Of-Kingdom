/* $Id: mp_login.cpp 50955 2011-08-30 19:41:15Z mordante $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/player_city.hpp"

#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/password_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include <hero.hpp>
#include "help.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "multiplayer.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_mp_login
 *
 * == Multiplayer connect ==
 *
 * This shows the dialog to log in to the MP server
 *
 * @begin{table}{dialog_widgets}
 *
 * user_name & & text_box & m &
 *         The login user name. $
 *
 * password & & text_box & m &
 *         The password. $
 *
 * remember_password & & toggle_button & o &
 *         A toggle button to offer to remember the password in the
 *         preferences. $
 *
 * password_reminder & & button & o &
 *         Request a password reminder. $
 *
 * change_username & & button & o &
 *         Use a different username. $
 *
 * login_label & & button & o &
 *         Displays the information received from the server. $
 *
 * @end{table}
 */

REGISTER_DIALOG(player_city)

tplayer_city::tplayer_city(game_display& disp, hero_map& heros)
	: disp_(disp)
	, heros_(heros)
{
}

const size_t max_name_size = 15;

void tplayer_city::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	ttext_box* user_widget = find_widget<ttext_box>(&window, "name", false, true);
	user_widget->set_value(group.city().name());
	user_widget->set_maximum_length(max_name_size);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "create", false)
		, boost::bind(
		&tplayer_city::create
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << help::tintegrate::generate_format(_("OK"), "blue");
	find_widget<tbutton>(&window, "create", false).set_label(strstr.str());
}

void tplayer_city::post_show(twindow& window)
{
}

bool is_valid_username2(const std::string& key, const std::string& str, game_display* disp)
{
	// #: chapter separator
	// |: section separator
	// <: tintegrate::help
	const char* invalids = "#|<";

	size_t pos = str.find_first_of(invalids);
	bool ret = pos == std::string::npos;
	if (!ret && disp) {
		std::stringstream strstr, err;
		utils::string_map symbols;

		std::string c;
		for (size_t p = 0; p < strlen(invalids); p ++) {
			if (!strstr.str().empty()) {
				strstr << ", ";
			}
			c.assign(invalids, p, 1);
			strstr << help::tintegrate::generate_format(c, "yellow");
		}

		symbols["key"] = help::tintegrate::generate_format(key, "red");
		symbols["invalids"] = strstr.str();
		
		err << vgettext("wesnoth-lib", "$key cannot include $invalids!", symbols);
		gui2::show_message(disp->video(), "", err.str());
	}
	return ret;
}

bool is_valid_username(const std::string& key, const std::string& str, game_display* disp)
{
	bool ret = utils::isvalid_username(str);
	if (!ret && disp) {
		std::stringstream strstr, err;
		utils::string_map symbols;

		const char* special = "_-";
		std::string c;
		for (size_t p = 0; p < strlen(special); p ++) {
			if (!strstr.str().empty()) {
				strstr << ", ";
			}
			c.assign(special, p, 1);
			strstr << help::tintegrate::generate_format(c, "yellow");
		}

		symbols["username"] = help::tintegrate::generate_format(key, "red");
		symbols["special"] = strstr.str();
		
		err << vgettext("wesnoth-lib", "$username may only use below special character: $special, and must not be all special!", symbols);
		gui2::show_message(disp->video(), "", err.str());
	}
	return ret;
}

std::string text_box_str(game_display& disp, twindow& window, const std::string& id, const std::string& name, int min, int max, bool username)
{
	std::stringstream err;
	utils::string_map symbols;

	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->get_value();

	if ((int)str.size() < min || (int)str.size() > max) {
		symbols["min"] = help::tintegrate::generate_format(min, "yellow");
		symbols["max"] = help::tintegrate::generate_format(max, "yellow");
		symbols["key"] = help::tintegrate::generate_format(name, "red");
		
		if (min != max) {
			err << vgettext("wesnoth-lib", "'$key' value must combine $min to $max characters", symbols);
		} else {
			err << vgettext("wesnoth-lib", "'$key' value must be $min characters", symbols);
		}
		gui2::show_message(disp.video(), "", err.str());
		return null_str;

	} else if (username) {
		if (!is_valid_username(name, str, &disp)) {
			return null_str;
		}
	}
	return str;
}

void tplayer_city::create(twindow& window)
{
	std::string name = text_box_str(disp_, window, "name", _("City name"), 1, max_name_size, true);
	if (name.empty()) {
		return;
	}

	if (name != group.city().name()) {
		std::map<int, std::string> block;
		block.insert(std::make_pair((int)http::block_tag_city, name));
		http::membership m = http::upload_data(disp_, heros_, block, false);
		if (m.uid >= 0) {
			group.from_local_membership(disp_, heros_, m, true);
		}
	}

	window.set_retval(twindow::OK);
}

} // namespace gui2

