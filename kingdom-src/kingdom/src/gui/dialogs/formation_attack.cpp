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

#include "gui/dialogs/formation_attack.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/listbox.hpp"
#include <boost/bind.hpp>
#include "game_display.hpp"
#include "team.hpp"
#include "unit_map.hpp"
#include "gamestatus.hpp"
#include "play_controller.hpp"
#include "resources.hpp"
#include "marked-up_text.hpp"
#include "gettext.hpp"

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

REGISTER_DIALOG(formation_attack)

tformation_attack::tformation_attack(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, unit& master, bool browse)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, master_(master)
	, candidate_()
	, selected_(-1)
	, browse_(browse)
{
	formation_ = calculate_attack_formation(teams, units, master, false);

	candidate_.push_back(formation_.u_);
	for (std::map<int, unit*>::const_iterator it = formation_.adjacent_.begin(); it != formation_.adjacent_.end(); ++ it) {
		// candidate_.push_back(*it);
	}

}

void tformation_attack::unit_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "tactic_list", false);
	selected_ = list.get_selected_row();
}

void tformation_attack::pre_show(CVideo& /*video*/, twindow& window)
{
	std::vector<hero*> captain;
	std::stringstream strstr;
	std::string color;

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(!browse_ && !formation_.disable_ && formation_can_attack(units_, teams_[master_.side() - 1], formation_));

	find_widget<tlabel>(&window, "title", false, true)->set_visible(twidget::INVISIBLE);

	tcontrol* control = find_widget<tcontrol>(&window, "icon", false, true);
	control->set_label(formation_.profile_->icon());

	tlabel* label = find_widget<tlabel>(&window, "name", false, true);
	label->set_label(formation_.profile_->name());

	label = find_widget<tlabel>(&window, "description", false, true);
	label->set_label(formation_.profile_->description());

	tlistbox& tactic_list =
			find_widget<tlistbox>(&window, "tactic_list", false);
	const team& current_team = teams_[master_.side() - 1];

	for (std::vector<unit*>::iterator it = candidate_.begin(); it != candidate_.end(); ++ it) {
		unit& u = **it;

		std::map<std::string, string_map> data;
		string_map item;

		strstr.str("");
		strstr << u.master().image();
		item["label"] = strstr.str();
		data.insert(std::make_pair("icon", item));

		strstr.str("");
		color = "red";
		strstr << tintegrate::generate_format(32, color, 16, true);
		item["label"] = strstr.str();
		data.insert(std::make_pair("point", item));

		item["label"] = u.master().image();
		data.insert(std::make_pair("name", item));

		strstr.str("");
		strstr << tintegrate::generate_format(u.skill_[hero_skill_formation], "blue", 16);
		item["label"] = strstr.str();
		data.insert(std::make_pair("turn", item));

		tactic_list.add_row(data);
	}
	tactic_list.set_visible(twidget::INVISIBLE);

	tactic_list.set_callback_value_change(dialog_callback<tformation_attack, &tformation_attack::unit_selected>);
	unit_selected(window);
}

void tformation_attack::post_show(twindow& window)
{
	if (get_retval() == twindow::OK) {
		selected_ = find_widget<tlistbox>(&window, "tactic_list", false).get_selected_row();
	}
}

}
