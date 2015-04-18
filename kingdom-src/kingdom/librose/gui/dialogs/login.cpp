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

#include "gui/dialogs/login.hpp"

#include "display.hpp"
#include "preferences.hpp"
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

REGISTER_DIALOG(login)

tlogin::tlogin(display& disp, hero_map& heros, const std::string& label)
	: disp_(disp)
	, heros_(heros)
	, orignal_username_(preferences::login())
	, orignal_password_(preferences::password())
	, registed_(false)
{
	register_label("login_label", false, label);
}

const size_t max_login_size = 15;

void tlogin::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream strstr;

	tlabel* label = find_widget<tlabel>(&window, "forum", false, true);
	strstr.str("");
	strstr << tintegrate::generate_format(_("Forum"), "green") << "  http://";
	strstr << game_config::bbs_server.host << game_config::bbs_server.url;
	label->set_label(strstr.str());

	ttext_box* user_widget = find_widget<ttext_box>(&window, "username", false, true);
	user_widget->set_label(preferences::login());
	user_widget->set_maximum_length(max_login_size);

	tpassword_box* pw = find_widget<tpassword_box>(&window, "password", false, true);
	pw->set_label(preferences::password());
	pw->set_maximum_length(max_login_size);

	user_widget = find_widget<ttext_box>(&window, "nick", false, true);
	user_widget->set_label(preferences::nick());
	user_widget->set_maximum_length(max_login_size);

	tcontrol* control = find_widget<tcontrol>(&window, "remember_password", false, true);
	control->set_visible(twidget::INVISIBLE);	

	utils::string_map symbols;
	tscroll_label* label2 = find_widget<tscroll_label>(&window, "remark", false, true);
	symbols["server"] = tintegrate::generate_format(game_config::bbs_server.name, "green");
	symbols["host"] = tintegrate::generate_format(game_config::bbs_server.host, "green");
	symbols["register"] = tintegrate::generate_format(_("Register"), "blue");
	symbols["ok"] = tintegrate::generate_format(_("OK"), "yellow");
	label2->set_label(vgettext("wesnoth-lib", "account^remark($server, $host, $register, $ok)", symbols));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "register", false)
		, boost::bind(
		&tlogin::register1
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Register"), "blue");
	find_widget<tbutton>(&window, "register", false).set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "create", false)
		, boost::bind(
		&tlogin::create
		, this
		, boost::ref(window)
		, (int)LOGIN));

	find_widget<tbutton>(&window, "cancel", false).set_visible(twidget::INVISIBLE);
}

void tlogin::post_show(twindow& window)
{
}

std::string tlogin::text_box_str(twindow& window, const std::string& id, const std::string& name, int min, int max, bool allow_empty)
{
	std::stringstream err;
	utils::string_map symbols;

	std::string str;
	if (id != "password") {
		ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
		str = widget->label();
	} else {
		tpassword_box* pd = find_widget<tpassword_box>(&window, "password", false, true);
		str = pd->get_real_value();
	}

	if (str.empty()) {
		if (!allow_empty) {
			symbols["key"] = tintegrate::generate_format(name, "red");
			
			err << vgettext("wesnoth-lib", "Invalid '$key' value, not accept empty", symbols);
			gui2::show_message(disp_.video(), "", err.str());
		}
		return str;

	} else if ((int)str.size() < min || (int)str.size() > max) {
		symbols["min"] = tintegrate::generate_format(min, "yellow");
		symbols["max"] = tintegrate::generate_format(max, "yellow");
		symbols["key"] = tintegrate::generate_format(name, "red");
		
		err << vgettext("wesnoth-lib", "'$key' value must combine $min to $max characters", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return null_str;

	} else if (id == "username") {
		if (!utils::isvalid_username(str)) {
			gui2::show_message(disp_.video(), "", utils::errstr);
			return null_str;
		} else if (game_config::is_reserve_player(str)) {
			symbols["key"] = tintegrate::generate_format(name, "red");
			symbols["username"] = tintegrate::generate_format(str, "red");
			err << vgettext("wesnoth-lib", "Invalid '$key' value, $username is reserved!", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return null_str;
		}
	} else if (id == "nick") {
		if (!utils::isvalid_nick(str)) {
			return null_str;
		}
	}
	return str;
}

bool tlogin::create(twindow& window, int operate)
{
	int min_username_char = (operate == REGISTER)? 6: 3;
	std::string username = text_box_str(window, "username", _("Name"), min_username_char, max_login_size * 4, false);
	if (username.empty()) {
		return false;
	}

	tpassword_box* pw = find_widget<tpassword_box>(&window, "password", false, true);
	std::string password = pw->get_real_value();
	if (!password.empty()) {
		std::string password = text_box_str(window, "password", _("Password"), 0, max_login_size * 4, true);
		if (password.empty()) {
			return false;
		}
	}

	std::string nick = text_box_str(window, "nick", _("Nick"), 0, max_login_size * 4, true);
	if (nick.empty() && !utils::isvalid_nick(username)) {
		std::stringstream err;
		utils::string_map symbols;

		symbols["username"] = tintegrate::generate_format(username, "yellow");
		err << vgettext("wesnoth-lib", "$username of username is not a valid nick, you require to set a valid nick separately!\n Nick can ony use alpha, number and _.", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return false;
	}
	preferences::set_nick(nick);

	preferences::set_remember_password(true);
	preferences::set_password(password);

	hero& h = group.leader();
	h.set_name(username);
	preferences::set_hero(heros_, h);

	lobby->set_nick2(group.leader().name());

	if (operate == LOGIN) {
		window.set_retval(twindow::OK);
	}
	return true;
}

void tlogin::register1(twindow& window)
{
	std::stringstream err;
	utils::string_map symbols;
	const int min_password_chars = 6;

	if (create(window, REGISTER)) {
		ttext_box* widget = find_widget<ttext_box>(&window, "validate_password", false, true);
		std::string validate_password = widget->label();
		if (validate_password != preferences::password()) {
			symbols["validate_password"] = tintegrate::generate_format(_("Validate password"), "red");
			symbols["password"] = tintegrate::generate_format(_("Password"), "red");
			err << vgettext("wesnoth-lib", "$validate_password isn't same as $password", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return;
		}
		if ((int)validate_password.size() < min_password_chars) {
			symbols["min"] = tintegrate::generate_format(min_password_chars, "yellow");
		
			err << vgettext("wesnoth-lib", "When register, password must be greater than or equal to $min characters", symbols);
			gui2::show_message(disp_.video(), "", err.str());
			return;
		}
		if (http::register_user(disp_, heros_, false)) {
			registed_ = true;
		}
	}
}

bool tlogin::dirty() const
{
	return registed_ || preferences::login() != orignal_username_ || preferences::password() != orignal_password_;
}

} // namespace gui2

