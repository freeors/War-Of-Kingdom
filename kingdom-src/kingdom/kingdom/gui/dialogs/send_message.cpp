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

#include "gui/dialogs/send_message.hpp"

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
#include "integrate.hpp"
#include "gettext.hpp"
#include "formula_string_utils.hpp"
#include "multiplayer.hpp"

#include <boost/bind.hpp>

namespace gui2 {

extern std::string parse_message_content(const std::string& content);

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

REGISTER_DIALOG(send_message)

tsend_message::tsend_message(display& disp, hero_map& heros, const std::string& initial_str, int mode)
	: disp_(disp)
	, heros_(heros)
	, initial_str_(initial_str)
	, mode_(mode)
	, receiver_str_()
	, result_str_()
	, max_size_(5000)
{
}

void tsend_message::pre_show(CVideo& /*video*/, twindow& window)
{
	window.set_canvas_variable("border", variant("default-border"));

	std::stringstream strstr;

	tlabel* label = find_widget<tlabel>(&window, "remark", false, true);
	strstr.str("");
	if (mode_ == SEND_MESSAGE) {
		strstr << _("Please input content wanted to send");
	} else {
		strstr << _("Please input content");
	}
	label->set_label(strstr.str());

	ttext_box* content_area = find_widget<ttext_box>(&window, "content_area", false, true);
	content_area->set_label(initial_str_);
	content_area->set_maximum_length(max_size_);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "format", false)
		, boost::bind(
		&tsend_message::format
		, this
		, boost::ref(window)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "create", false)
		, boost::bind(
		&tsend_message::create
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("OK"), "blue");
	find_widget<tbutton>(&window, "create", false).set_label(strstr.str());

	tscroll_label* clone = find_widget<tscroll_label>(&window, "clone", false, true);
	clone->set_label(parse_message_content(initial_str_));
}

void tsend_message::post_show(twindow& window)
{
}

bool is_valid_content(const std::string& key, const std::string& str, display* disp)
{
	// #: chapter separator
	// |: section separator
	// &: filter separator
	const char* invalids = "#|&";

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
			strstr << tintegrate::generate_format(c, "yellow");
		}

		symbols["key"] = tintegrate::generate_format(key, "red");
		symbols["invalids"] = strstr.str();
		
		err << vgettext2("$key cannot include $invalids!", symbols);
		gui2::show_message(disp->video(), "", err.str());
	}
	return ret;
}

std::string text_box_str2(display& disp, twindow& window, const std::string& id, const std::string& name, int min, int max)
{

	std::stringstream err;
	utils::string_map symbols;

	ttext_box* widget = find_widget<ttext_box>(&window, id, false, true);
	std::string str = widget->label();

	if ((int)str.size() < min || (int)str.size() > max) {
		symbols["min"] = tintegrate::generate_format(min, "yellow");
		symbols["max"] = tintegrate::generate_format(max, "yellow");
		symbols["key"] = tintegrate::generate_format(name, "red");
		
		if (min != max) {
			err << vgettext2("'$key' value must combine $min to $max characters", symbols);
		} else {
			err << vgettext2("'$key' value must be $min characters", symbols);
		}
		gui2::show_message(disp.video(), "", err.str());
		return null_str;

	}
	if (!is_valid_content(name, str, &disp)) {
		return null_str;
	}
	return str;
}

void tsend_message::format(twindow& window)
{
	ttext_box* widget = find_widget<ttext_box>(&window, "content_area", false, true);
	std::string content = widget->label();

	tscroll_label* clone = find_widget<tscroll_label>(&window, "clone", false, true);
	clone->set_label(parse_message_content(content));	
}

void tsend_message::create(twindow& window)
{
	receiver_str_ = text_box_str2(disp_, window, "receiver", _("Receiver"), 1, max_size_);
	if (receiver_str_.empty()) {
		return;
	}
	// std::string content = text_box_str2(disp_, window, "content_area", _("Content area"), 1, max_size_);
	ttext_box* widget = find_widget<ttext_box>(&window, "content_area", false, true);
	result_str_ = widget->label();
	if (result_str_.empty()) {
		return;
	}

	window.set_retval(twindow::OK);
}

} // namespace gui2

