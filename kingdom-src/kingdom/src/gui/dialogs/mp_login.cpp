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

#include "gui/dialogs/mp_login.hpp"

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

REGISTER_DIALOG(mp_login)

tmp_login::tmp_login(game_display& disp, const std::string& label)
	: disp_(disp)
{
	register_label("login_label", false, label);
}

const size_t max_login_size = 20;

void tmp_login::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	ttext_box* user_widget = find_widget<ttext_box>(&window, "username", false, true);
	user_widget->set_value(preferences::login());
	user_widget->set_maximum_length(max_login_size);

	user_widget = find_widget<ttext_box>(&window, "password", false, true);
	user_widget->set_value(preferences::password());
	user_widget->set_maximum_length(max_login_size);

	tcontrol* control = find_widget<tcontrol>(&window, "remember_password", false, true);
	control->set_visible(twidget::INVISIBLE);	

	utils::string_map symbols;
	tlabel* label = find_widget<tlabel>(&window, "remark", false, true);
	symbols["server"] = help::tintegrate::generate_format(game_config::bbs_server.name, "green");
	symbols["host"] = help::tintegrate::generate_format(game_config::bbs_server.host, "green");
	symbols["register"] = help::tintegrate::generate_format(_("Register"), "blue");
	symbols["ok"] = help::tintegrate::generate_format(_("OK"), "yellow");
	label->set_label(vgettext("wesnoth-lib", "account^remark($server, $host, $register, $ok)", symbols));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "register", false)
		, boost::bind(
		&tmp_login::register1
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << help::tintegrate::generate_format(_("Register"), "blue");
	find_widget<tbutton>(&window, "register", false).set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "create", false)
		, boost::bind(
		&tmp_login::create
		, this
		, boost::ref(window)
		, true));

	find_widget<tbutton>(&window, "cancel", false).set_visible(twidget::INVISIBLE);
}

void tmp_login::post_show(twindow& window)
{
}

std::string tmp_login::text_box_str(twindow& window, const std::string& id, const std::string& name, int min, int max)
{
	std::stringstream err;
	utils::string_map symbols;

	std::string str;
	if (id != "password") {
		ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
		str = widget->get_value();
	} else {
		tpassword_box* pd = find_widget<tpassword_box>(&window, "password", false, true);
		str = pd->get_real_value();
	}

	if (str.empty()) {
		symbols["key"] = help::tintegrate::generate_format(name, "red");
		
		err << vgettext("wesnoth-lib", "Invalid '$key' value, not accept empty", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return str;
	} else if ((int)str.size() < min || (int)str.size() > max) {
		symbols["min"] = help::tintegrate::generate_format(min, "yellow");
		symbols["max"] = help::tintegrate::generate_format(max, "yellow");
		symbols["key"] = help::tintegrate::generate_format(name, "red");
		
		err << vgettext("wesnoth-lib", "'$key' value must combine $min to $max characters", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return null_str;

	} else if (id == "username") {
		if (str.find("<") != std::string::npos) {
			symbols["str"] = name;
			symbols["char"] = "<";
			err << vgettext("wesnoth-lib", "Include unsupportable character: $char, invalid '$str' value", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return null_str;
		} else if (id == "username" && game_config::is_reserve_player(str)) {
			symbols["key"] = help::tintegrate::generate_format(name, "red");
			symbols["username"] = help::tintegrate::generate_format(preferences::login(), "red");
			err << vgettext("wesnoth-lib", "Invalid '$key' value, $username is reserved!", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return null_str;
		}
	}
	return str;
}

bool tmp_login::create(twindow& window, bool close)
{
	std::string username = text_box_str(window, "username", _("Name"), 3, 15);
	if (username.empty()) {
		return false;
	}

	std::string password = text_box_str(window, "password", _("Password"), 6, 15);
	if (password.empty()) {
		return false;
	}

	preferences::set_remember_password(true);
	preferences::set_password(password);

	hero& h = group.leader();
	h.set_name(username);
	preferences::set_hero(h);
	if (close) {
		window.set_retval(twindow::OK);
	}
	return true;
}

void tmp_login::register1(twindow& window)
{
	std::stringstream err;
	utils::string_map symbols;

	if (create(window, false)) {
		ttext_box* widget = find_widget<ttext_box>(&window, "validate_password", false, true);
		std::string validate_password = widget->get_value();
		if (validate_password != preferences::password()) {
			symbols["validate_password"] = help::tintegrate::generate_format(_("Validate password"), "red");
			symbols["password"] = help::tintegrate::generate_format(_("Password"), "red");
			err << vgettext("wesnoth-lib", "$validate_password isn't same as $password", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return;
		}
		http::register_user(disp_, false);
	}
}

} // namespace gui2

