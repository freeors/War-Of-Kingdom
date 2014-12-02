/* $Id: game_save.cpp 48937 2011-03-19 21:04:08Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Jörg Hinrichs <joerg.hinrichs@alice-dsl.de>
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

#include "gui/dialogs/give.hpp"

#include "game_display.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "gui/dialogs/field.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/dialogs/message.hpp"
#include <preferences.hpp>

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_save
 *
 * == Save a game ==
 *
 * This shows the dialog to create a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * lblTitle & & label & m &
 *         The title of the window. $
 *
 * txtFilename & & text_box & m &
 *         The name of the savefile. $
 *
 * @end{table}
 */

REGISTER_DIALOG(give)

tgive::tgive(game_display& disp, hero_map& heros, http::membership& target)
	: disp_(disp)
	, heros_(heros)
	, target_(target)
{

}

void tgive::refresh_summary(twindow& window) const
{
	utils::string_map symbols;
	std::stringstream strstr;

	tlabel* label = find_widget<tlabel>(&window, "from_description", false, true);
	
	symbols["username"] = tintegrate::generate_format(_("You"), "red");
	strstr.str("");
	strstr << tintegrate::generate_format(preferences::coin(), "yellow") << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)");
	symbols["coin"] = strstr.str();
	strstr.str("");
	strstr << tintegrate::generate_format(preferences::score(), "yellow") << tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
	symbols["score"] = strstr.str();
	strstr.str("");
	strstr << vgettext("wesnoth-lib", "$username has $coin and $score", symbols);
	label->set_label(strstr.str());

	label = find_widget<tlabel>(&window, "to_description", false, true);
	
	symbols["username"] = tintegrate::generate_format(target_.name, "green");
	strstr.str("");
	strstr << tintegrate::generate_format(target_.coin, "yellow") << tintegrate::generate_img("misc/coin.png~SCALE(24, 24)");
	symbols["coin"] = strstr.str();
	strstr.str("");
	strstr << tintegrate::generate_format(target_.score, "yellow") << tintegrate::generate_img("misc/score.png~SCALE(24, 24)");
	symbols["score"] = strstr.str();
	strstr.str("");
	strstr << vgettext("wesnoth-lib", "$username has $coin and $score", symbols);
	label->set_label(strstr.str());

	ttext_box* widget = find_widget<ttext_box>(&window, "coin", false, true);
	widget->set_value("0");

	widget = find_widget<ttext_box>(&window, "score", false, true);
	widget->set_value("0");
}

void tgive::pre_show(CVideo& video, twindow& window)
{
	std::stringstream strstr;
	refresh_summary(window);

	tlabel* label = find_widget<tlabel>(&window, "coin_unit", false, true);
	label->set_label(tintegrate::generate_img("misc/coin.png~SCALE(24, 24)"));

	label = find_widget<tlabel>(&window, "score_unit", false, true);
	label->set_label(tintegrate::generate_img("misc/score.png~SCALE(24, 24)"));

	tbutton* button = find_widget<tbutton>(&window, "do_give", false, true);
	connect_signal_mouse_left_click(
		*button
		, boost::bind(
		&tgive::do_give
		, this
		, boost::ref(window)));
	strstr.str("");
	strstr << tintegrate::generate_format(_("Give"), "blue");
	button->set_label(strstr.str());
}

void tgive::do_give(twindow& window)
{
}

} // namespace gui2

