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

#include "gui/dialogs/signin.hpp"

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

REGISTER_DIALOG(signin)

tsignin::tsignin(game_display& disp, hero_map& heros)
	: disp_(disp)
	, heros_(heros)
{
}

void tsignin::refresh_signin_information(twindow& window) const
{
	std::stringstream strstr;
	utils::string_map symbols;

	tlabel* label = find_widget<tlabel>(&window, "remark", false, true);
	symbols["continue"] = help::tintegrate::generate_format(group.signin().continue_days, "green");
	symbols["break"] = help::tintegrate::generate_format(group.signin().break_days, "red");
	symbols["have"] = group.signin().today? dsgettext("wesnoth-lib", "been^have"): dsgettext("wesnoth-lib", "been^haven't");
	strstr.str("");
	strstr << vgettext("wesnoth-lib", "You have been sign in $continue days, broken $break days. Today $have sign in.", symbols);
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "fillup_remark", false, true);
	tbutton* button = find_widget<tbutton>(&window, "fillup", false, true);
	if (group.signin().break_days) {
		symbols.clear();
		symbols["ps"] = help::tintegrate::generate_format(dsgettext("wesnoth-lib", "PS"), "red");
		symbols["break"] = help::tintegrate::generate_format(group.signin().break_days, "red");
		symbols["max_break"] = help::tintegrate::generate_format(game_config::max_breaks, "yellow");
		symbols["fillup"] = help::tintegrate::generate_format(_("signin^Fill up"), "blue");

		strstr.str("");
		strstr << vgettext("wesnoth-lib", "fill up sign in($ps, $break, $max_break, $fillup)", symbols);
		label->set_label(strstr.str());

		strstr.str("");
		strstr << help::tintegrate::generate_format(_("signin^Fill up"), "blue");
		button->set_label(strstr.str());
	} else {
		label->set_visible(twidget::INVISIBLE);
		button->set_visible(twidget::INVISIBLE);
	}
}

void tsignin::pre_show(CVideo& /*video*/, twindow& window)
{
	refresh_signin_information(window);

	std::stringstream strstr;
	utils::string_map symbols;

	strstr.str("");
	symbols["fillup"] = help::tintegrate::generate_format(_("signin^Fill up"), "blue");
	strstr << vgettext("wesnoth-lib", "sign in remark($fillup)", symbols);
	tlabel* label = find_widget<tlabel>(&window, "signin_remark", false, true);
	label->set_label(strstr.str());

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "fillup", false)
		, boost::bind(
		&tsignin::fillup
		, this
		, boost::ref(window)));
}

void tsignin::post_show(twindow& window)
{
}

void tsignin::fillup(twindow& window)
{
	if (!group.signin().break_days) {
		return;
	}

	utils::string_map symbols;
	std::stringstream err;
	int coin = 0;
	int score = 200;

	symbols["score"] = help::tintegrate::generate_format(score, "red");
	symbols["do"] = help::tintegrate::generate_format(dsgettext("wesnoth-lib", "signin^Fill up"), "yellow");
	std::string message = vgettext("wesnoth-lib", "Do you want to spend $score score to $do?", symbols);
	int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
	if (res == gui2::twindow::CANCEL) {
		return;
	}

	if (sum_score(group.coin(), group.score()) < score) {
		symbols.clear();
		symbols["coin"] = help::tintegrate::generate_format(coin, "red");
		symbols["score"] = help::tintegrate::generate_format(score, "red");
		err << vgettext("wesnoth-lib", "Repertory is not enough to pay $coin coin and $score score. If lack one only, can exchange between coin and score.", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return;
	}

	http::membership m = http::signin(disp_, heros_, http::signin_tag_fillup);
	if (m.uid >= 0) {
		group.from_local_membership(disp_, heros_, m, false);
		refresh_signin_information(window);
	}
}

} // namespace gui2

