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

#include "gui/dialogs/tactic.hpp"

#include "foreach.hpp"
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

REGISTER_DIALOG(tactic)

ttactic::ttactic(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, unit& tactician, bool cast)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, tactician_(tactician)
	, cast_(cast)
	, candidate_()
	, valid_()
	, cannot_valid_(false)
	, selected_(-1)
{
	if (tactician_.master().tactic_ != HEROS_NO_TACTIC) {
		candidate_.push_back(&tactician_.master());
	}
	if (tactician_.second().valid() && tactician_.second().tactic_ != HEROS_NO_TACTIC) {
		candidate_.push_back(&tactician_.second());
	}
	if (tactician_.third().valid() && tactician_.third().tactic_ != HEROS_NO_TACTIC) {
		candidate_.push_back(&tactician_.third());
	}
	if (!cast) {
		std::vector<unit*> actives = teams_[tactician.side() - 1].active_tactics();
		if ((int)actives.size() >= game_config::active_tactic_slots || 
			std::find(actives.begin(), actives.end(), &tactician) != actives.end()) {
			cannot_valid_ = true;
		}
	}
}

void ttactic::tactic_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "tactic_list", false);
	selected_ = list.get_selected_row();

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(valid_[selected_]);
}

void ttactic::pre_show(CVideo& /*video*/, twindow& window)
{
	std::vector<hero*> captain;
	std::stringstream strstr;
	std::string color;

	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	if (cast_) {
		title->set_label(_("Cast tactic"));
		ok->set_label(_("tactic^Cast"));
	} else {
		title->set_label(_("Active tactic"));
		ok->set_label(_("tactic^Active"));
	}

	tlistbox& tactic_list =
			find_widget<tlistbox>(&window, "tactic_list", false);
	const team& current_team = teams_[tactician_.side() - 1];
	int side_point = current_team.tactic_point();
	std::map<int, std::vector<map_location> > touched;
	bool has_effect_unit;

	for (std::vector<hero*>::iterator it = candidate_.begin(); it != candidate_.end(); ++ it) {
		hero& h = **it;
		const ::ttactic& t = unit_types.tactic(h.tactic_);

		std::map<std::string, string_map> data;
		string_map item;

		has_effect_unit = false;
		touched = t.touch_units(units_, tactician_);
		for (std::map<int, std::vector<map_location> >::const_iterator it2 = touched.begin(); it2 != touched.end(); ++ it2) {
			if (!it2->second.empty()) {
				has_effect_unit =true;
				break;
			}
		}

		strstr.str("");
		strstr << h.image();
		if (cast_ && (tactician_.provoked_turns() || !has_effect_unit)) {
			strstr << "~GS()";
		}
		item["label"] = strstr.str();
		data.insert(std::make_pair("icon", item));

		strstr.str("");
		if (side_point < t.point()) {
			color = "red";
		} else {
			color = "";
		}
		if (!cast_) {
			strstr << "--/";
		}
		strstr << help::tintegrate::generate_format(t.point(), color, 16, true);
		item["label"] = strstr.str();
		data.insert(std::make_pair("point", item));

		item["label"] = t.name();
		data.insert(std::make_pair("name", item));

		strstr.str("");
		strstr << help::tintegrate::generate_format(::ttactic::calculate_turn(fxptoi9(h.force_), fxptoi9(h.intellect_)), "blue", 16);
		item["label"] = strstr.str();
		data.insert(std::make_pair("turn", item));

		if (cannot_valid_) {
			valid_.push_back(false);
		} else if (cast_) {
			valid_.push_back(!tactician_.provoked_turns() && has_effect_unit && side_point >= t.point());
		} else {
			valid_.push_back(!tactician_.provoked_turns());
		}
		tactic_list.add_row(data);
	}

	tactic_list.set_callback_value_change(dialog_callback<ttactic, &ttactic::tactic_selected>);

	tactic_selected(window);
}

void ttactic::post_show(twindow& window)
{
	if (get_retval() == twindow::OK) {
		selected_ = find_widget<tlistbox>(&window, "tactic_list", false).get_selected_row();
	}
}

hero* ttactic::get_selected_hero() const
{
	if (selected_ < 0) {
		return &hero_invalid;
	}
	return candidate_[selected_]; 
}

}
