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

#include "gui/dialogs/list.hpp"

#include "foreach.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/button.hpp"
#include "gui/dialogs/side_list.hpp"
#include "gui/dialogs/strategy_list.hpp"
#include "gui/dialogs/troop_list.hpp"
#include "gui/dialogs/city_list.hpp"
#include "gui/dialogs/artifical_list.hpp"
#include "gui/dialogs/hero_list.hpp"
#include <boost/bind.hpp>
#include "game_display.hpp"
#include "team.hpp"
#include "unit_map.hpp"
#include "hero.hpp"
#include "gamestatus.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "game_events.hpp"
#include "dialogs.hpp"

// extern std::vector<hero*> empty_vector_hero_ptr;

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

REGISTER_DIALOG(list)

tlist::tlist(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, game_state& gamestate, int side)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, gamestate_(gamestate)
	, current_team_(teams[side - 1])
{
	
}

void tlist::pre_show(CVideo& /*video*/, twindow& window)
{
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "side_0", false)
		, boost::bind(
			&tlist::side
			, this
			, boost::ref(window)
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "objectives_1", false)
		, boost::bind(
			&tlist::objectives
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "strategy_0", false)
		, boost::bind(
			&tlist::strategy_list
			, this
			, boost::ref(window)
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "strategy_1", false)
		, boost::bind(
			&tlist::strategy_list
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "troop_0", false)
		, boost::bind(
			&tlist::troop
			, this
			, boost::ref(window)
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "troop_1", false)
		, boost::bind(
			&tlist::troop
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "city_0", false)
		, boost::bind(
			&tlist::city
			, this
			, boost::ref(window)
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "city_1", false)
		, boost::bind(
			&tlist::city
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "artifical_0", false)
		, boost::bind(
			&tlist::artifical_list
			, this
			, boost::ref(window)
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "artifical_1", false)
		, boost::bind(
			&tlist::artifical_list
			, this
			, boost::ref(window)
			, false));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "hero_0", false)
		, boost::bind(
			&tlist::hero_list
			, this
			, boost::ref(window)
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "hero_1", false)
		, boost::bind(
			&tlist::hero_list
			, this
			, boost::ref(window)
			, false));
}

void tlist::post_show(twindow& window)
{
}

void tlist::objectives(twindow& window, bool global)
{
	play_controller* controller = resources::controller;

	int side_num = current_team_.side();
	if (global) {
		side_num = -1;
		return;
	}

	config cfg;
	cfg["side"] = side_num;
	game_events::handle_event_command("show_objectives", game_events::queued_event("_from_interface", map_location(),
			map_location(), config()), vconfig(cfg));

	dialogs::show_objectives(controller->level(), current_team_.objectives());
	current_team_.reset_objectives_changed();
}

void tlist::side(twindow& window, bool global)
{
	int side_num = current_team_.side();
	if (global) {
		side_num = -1;
	}
	gui2::tside_list dlg(gui_, teams_, units_, heros_, gamestate_);
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return ;
	}
}

void tlist::strategy_list(twindow& window, bool global)
{
	int side_num = current_team_.side();
	if (global) {
		side_num = -1;
	}
	gui2::tstrategy_list dlg(gui_, teams_, units_, heros_, side_num);
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

void tlist::troop(twindow& window, bool global)
{
	int side_num = current_team_.side();
	if (global) {
		side_num = -1;
	}
	gui2::ttroop_list dlg(gui_, teams_, units_, heros_, gamestate_, side_num);
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

void tlist::city(twindow& window, bool global)
{
	int side_num = current_team_.side();
	if (global) {
		side_num = -1;
	}
	gui2::tcity_list dlg(gui_, teams_, units_, heros_, gamestate_, side_num);
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

void tlist::artifical_list(twindow& window, bool global)
{
	int side_num = current_team_.side();
	if (global) {
		side_num = -1;
	}
	gui2::tartifical_list dlg(gui_, teams_, units_, heros_, gamestate_, side_num);
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

void tlist::hero_list(twindow& window, bool global)
{
	std::vector<hero*> v;
	if (!global) {
		int side_index = current_team_.side() - 1;
		for (hero_map::iterator h = heros_.begin(); h != heros_.end(); ++ h) {
			if (h->side_ != side_index) {
				continue;
			}
			v.push_back(&*h);
		}
	}
	gui2::thero_list dlg(&teams_, &units_, heros_, v);
	try {
		dlg.show(gui_.video());
	} catch(twml_exception& e) {
		e.show(gui_);
		return;
	}
}

}
