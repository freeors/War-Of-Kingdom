/* $Id: player_selection.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/strategy_list.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/dialogs/unit_merit.hpp"

#include <boost/bind.hpp>

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_game_load
 *
 * == Load a game ==
 *
 * This shows the dialog to select and load a savegame file.
 *
 * @begin{table}{dialog_widgets}
 *
 * txtFilter & & text & m &
 *         The filter for the listbox items. $
 *
 * savegame_list & & listbox & m &
 *         List of savegames. $
 *
 * -filename & & control & m &
 *         Name of the savegame. $
 *
 * -date & & control & o &
 *         Date the savegame was created. $
 *
 * preview_pane & & widget & m &
 *         Container widget or grid that contains the items for a preview. The
 *         visible status of this container depends on whether or not something
 *         is selected. $
 *
 * -minimap & & minimap & m &
 *         Minimap of the selected savegame. $
 *
 * -imgLeader & & image & m &
 *         The image of the leader in the selected savegame. $
 *
 * -lblScenario & & label & m &
 *         The name of the scenario of the selected savegame. $
 *
 * -lblSummary & & label & m &
 *         Summary of the selected savegame. $
 *
 * @end{table}
 */

REGISTER_DIALOG(strategy_list)

tstrategy_list::tstrategy_list(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, int side)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, side_(side)
	, troop_index_(0)
	, hero_table_(NULL)
	, strategy_list_()
{
}

void tstrategy_list::type_selected(twindow& window, tlistbox& list, const int type)
{
	troop_index_ = list.get_selected_row();
	refresh_tooltip(window);
}

void tstrategy_list::refresh_tooltip(twindow& window)
{
	std::stringstream str;

	if (strategy_list_.empty()) {
		tlabel* label = find_widget<tlabel>(&window, "tip_side", false, true);
		str << _("Side") << ": ";
		label->set_label(str.str());

		str.str("");
		label = find_widget<tlabel>(&window, "tip_target", false, true);
		str << _("Target") << ": ";
		label->set_label(str.str());

		label = find_widget<tlabel>(&window, "tip_type", false, true);
		str << _("Type") << ": ";
		label->set_label(str.str());

		// turns
		str.str("");
		label = find_widget<tlabel>(&window, "tip_turns", false, true);
		str << _("Left turns") << ": ";
		label->set_label(str.str());

		// allies
		str.str("");
		label = find_widget<tlabel>(&window, "tip_ally", false, true);
		str << _("Ally") << ": ";
		label->set_label(str.str());
		return;
	}

	const std::pair<int, int>& index = strategy_list_[troop_index_];
	const strategy& s = teams_[index.first].strategies()[index.second];
	artifical* target_city = units_.city_from_cityno(s.target_);

	// refresh to gui
	tlabel* label = find_widget<tlabel>(&window, "tip_side", false, true);
	str << _("Side") << ": " << teams_[index.first].name();
	label->set_label(str.str());

	str.str("");
	label = find_widget<tlabel>(&window, "tip_target", false, true);
	str << _("Target") << ": " << target_city->name();
	label->set_label(str.str());

	// type
	str.str("");
	label = find_widget<tlabel>(&window, "tip_type", false, true);
	str << _("Type") << ": ";
	if (s.type_ == strategy::AGGRESS) {
		str << _("Aggress");
	} else if (s.type_ == strategy::DEFEND) {
		str << _("Defend");
	} else {
		str << _("---");
	}
	label->set_label(str.str());

	// turns
	str.str("");
	label = find_widget<tlabel>(&window, "tip_turns", false, true);
	str << _("Left turns") << ": " << s.impletement_turns_;
	label->set_label(str.str());

	// allies
	str.str("");
	label = find_widget<tlabel>(&window, "tip_ally", false, true);
	str << _("Ally") << ": ";
	for (std::set<int>::const_iterator it = s.allies_.begin(); it != s.allies_.end(); ++ it) {
		if (it != s.allies_.begin()) {
			str << ("  ");
		}
		str << teams_[*it - 1].name();
	}
	label->set_label(str.str());

}

void tstrategy_list::pre_show(CVideo& /*video*/, twindow& window)
{
	std::stringstream str;
	
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (size_t i = 0; i < teams_.size(); i ++) {
		// bool survived = units_.side_survived(i + 1);
		bool survived = true;
		if (!survived || (side_ != -1 && i != side_ - 1)) {
			continue;
		}
		const team& current_team = teams_[i];
		const std::vector<strategy>& strategies = current_team.strategies();
		int strategy_index = 0;
		for (std::vector<strategy>::const_iterator it = strategies.begin(); it != strategies.end(); ++ it, strategy_index ++) {
			const strategy& s = *it;
			artifical* target_city = units_.city_from_cityno(s.target_);

			// Add list item
			string_map list_item;
			std::map<std::string, string_map> list_item_item;

			// side
			str.str("");
			str << current_team.name();
			list_item["label"] = str.str();
			list_item_item.insert(std::make_pair("list_side", list_item));

			// target
			str.str("");
			str << target_city->name();
			list_item["label"] = str.str();
			list_item_item.insert(std::make_pair("list_target", list_item));

			// type
			str.str("");
			if (s.type_ == strategy::AGGRESS) {
				str << _("Aggress");
			} else if (s.type_ == strategy::DEFEND) {
				str << _("Defend");
			} else {
				str << _("---");
			}
			list_item["label"] = str.str();
			list_item_item.insert(std::make_pair("list_type", list_item));

			// turns
			str.str("");
			str << s.impletement_turns_;
			list_item["label"] = str.str();
			list_item_item.insert(std::make_pair("list_turns", list_item));

			// ally
			str.str("");
			str << s.allies_.size();
			list_item["label"] = str.str();
			list_item_item.insert(std::make_pair("list_ally", list_item));

			list->add_row(list_item_item);
			strategy_list_.push_back(std::make_pair(i, strategy_index));
		}

	}
	list->set_callback_value_change(dialog_callback3<tstrategy_list, tlistbox, &tstrategy_list::type_selected>);

	refresh_tooltip(window);
}

void tstrategy_list::post_show(twindow& window)
{
}

} // namespace gui2
