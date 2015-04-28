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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/alipay.hpp"

#include "game_display.hpp"
#include "game_preferences.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/message.hpp"
#include <hero.hpp>
#include "integrate.hpp"
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

REGISTER_DIALOG(alipay)

talipay::talipay(display& disp, hero_map& heros, const std::string& label, int inapp)
	: disp_(disp)
	, heros_(heros)
	, inapp_(inapp)
{
	// register_label("login_label", false, label);
}

const size_t max_text_size = 30;

void talipay::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::stringstream strstr;

	ttext_box* user_widget = find_widget<ttext_box>(&window, "number", false, true);
	user_widget->set_label("");
	user_widget->set_maximum_length(max_text_size);
	user_widget->set_visible(twidget::INVISIBLE);

	user_widget = find_widget<ttext_box>(&window, "buyer", false, true);
	user_widget->set_label("");
	user_widget->set_maximum_length(max_text_size);
	user_widget->set_visible(twidget::INVISIBLE);

	strstr.str("");
	utils::string_map symbols;
	tscroll_label* scroll_label = find_widget<tscroll_label>(&window, "remark", false, true);
	symbols["alipay"] = tintegrate::generate_format(_("Alipay"), "green");
	symbols["mail"] = tintegrate::generate_format(game_config::sales_email, "green");
	symbols["money"] = tintegrate::generate_format(15, "green");
	symbols["pay"] = tintegrate::generate_format(_("Pay"), "blue");
	strstr << vgettext2("payment^course($alipay, $mail, $money, $pay)", symbols);

	symbols.clear();
	symbols["ps"] = tintegrate::generate_format(_("PS"), "green");
	symbols["alipay"] = tintegrate::generate_format(_("Alipay"), "green");
	symbols["mail"] = tintegrate::generate_format(game_config::sales_email, "green");
	symbols["pay"] = tintegrate::generate_format(_("Pay"), "blue");

	strstr << "\n\n" << vgettext2("payment^pay remark($ps, $alipay, $mail, $pay)", symbols);
	scroll_label->set_label(strstr.str());

	symbols.clear();
	symbols["hotkey"] = tintegrate::generate_format("CTRL+V", "yellow");
	tlabel* label = find_widget<tlabel>(&window, "number_tag", false, true);
	strstr.str("");
	strstr << _("Transaction number");
	strstr << "(" << vgettext2("When input, may use $hotkey", symbols) << ")";
	label->set_label(strstr.str());
	label->set_visible(twidget::INVISIBLE);

	label = find_widget<tlabel>(&window, "buyer_tag", false, true);
	strstr.str("");
	strstr << _("Transaction buyer");
	strstr << "(" << vgettext2("When input, may use $hotkey", symbols) << ")";
	label->set_label(strstr.str());
	label->set_visible(twidget::INVISIBLE);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "pay", false)
		, boost::bind(
		&talipay::pay
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Pay"), "blue");
	find_widget<tbutton>(&window, "pay", false).set_label(strstr.str());
	find_widget<tbutton>(&window, "pay", false).set_visible(twidget::INVISIBLE);
}

void talipay::post_show(twindow& window)
{
}

std::string talipay::text_box_str(twindow& window, const std::string& id, const std::string& name, int min, int max, bool allow_empty)
{
	std::stringstream err;
	utils::string_map symbols;

	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->label();

	if (!allow_empty && str.empty()) {
		symbols["key"] = tintegrate::generate_format(name, "red");
		
		err << vgettext2("Invalid '$key' value, not accept empty", symbols);
		gui2::show_message(disp_.video(), "", err.str());
		return str;
	} else if ((int)str.size() < min || (int)str.size() > max) {
		symbols["min"] = tintegrate::generate_format(min, "yellow");
		symbols["max"] = tintegrate::generate_format(max, "yellow");
		symbols["key"] = tintegrate::generate_format(name, "red");
		
		if (min != max) {
			err << vgettext2("'$key' value must combine $min to $max characters", symbols);
		} else {
			err << vgettext2("'$key' value must be $min characters", symbols);
		}
		gui2::show_message(disp_.video(), "", err.str());
		return null_str;

	}
	return str;
}

void talipay::pay(twindow& window)
{
	std::string number = text_box_str(window, "number", _("Transaction number"), 28, 28, false);
	if (number.empty()) {
		return;
	}

	std::string buyer = text_box_str(window, "buyer", _("Transaction buyer"), 19, 19, false);
	if (buyer.empty()) {
		return;
	}

	http::membership m = http::insert_transaction(disp_, heros_, number, buyer, group.leader().uid(), inapp_);
	if (m.uid < 0) {
		return;
	}
	group.from_local_membership(disp_, heros_, m, false);

	utils::string_map symbols;
	std::stringstream strstr;
	symbols["do"] = tintegrate::generate_format(_("transaction^Insert"), "yellow");
	strstr << vgettext2("$do successfully!", symbols);
	gui2::show_message(disp_.video(), "", strstr.str());

	window.set_retval(twindow::OK);
}

} // namespace gui2
/*
horizontal_scrollbar_mode = "never"
width = "(screen_width + {WINDOW_WIDTH} - {PORTRAIT_WIDTH} - 30)"
height = "(screen_height)"
*/