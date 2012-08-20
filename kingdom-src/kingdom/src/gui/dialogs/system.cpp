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

#include "gui/dialogs/system.hpp"

#include "foreach.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include <boost/bind.hpp>
#include "game_display.hpp"
#include "team.hpp"
#include "unit_map.hpp"
#include "hero.hpp"
#include "gamestatus.hpp"
#include "play_controller.hpp"
#include "resources.hpp"

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

REGISTER_DIALOG(system)

tsystem::tsystem(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, gamestate_(gamestate)
	, retval_(NONE)
{
	
}

void tsystem::pre_show(CVideo& /*video*/, twindow& window)
{
	play_controller* controller = resources::controller;

	if (controller->is_replaying()) {
		find_widget<tbutton>(&window, "save", false).set_active(false);
		find_widget<tbutton>(&window, "savereplay", false).set_active(false);
	} else {
		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "save", false)
			, boost::bind(
				&tsystem::set_retval
				, this
				, boost::ref(window)
				, (int)SAVE));
		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "savereplay", false)
			, boost::bind(
				&tsystem::set_retval
				, this
				, boost::ref(window)
				, (int)SAVEREPLAY));
	}
/*
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "savemap", false)
		, boost::bind(
			&tsystem::set_retval
			, this
			, boost::ref(window)
			, (int)SAVEMAP));
*/
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "load", false)
		, boost::bind(
			&tsystem::set_retval
			, this
			, boost::ref(window)
			, (int)LOAD));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "preferences", false)
		, boost::bind(
			&tsystem::set_retval
			, this
			, boost::ref(window)
			, (int)PREFERENCES));
	if (game_config::tiny_gui) {
		find_widget<tbutton>(&window, "help", false).set_visible(twidget::INVISIBLE);
	} else {
		connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "help", false)
			, boost::bind(
				&tsystem::set_retval
				, this
				, boost::ref(window)
				, (int)HELP));
	}
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "quit", false)
		, boost::bind(
			&tsystem::set_retval
			, this
			, boost::ref(window)
			, (int)QUIT));
}

void tsystem::post_show(twindow& window)
{
}

void tsystem::set_retval(twindow& window, int val)
{
	retval_ = val;
	window.close();
}

}
