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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/final_battle.hpp"

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
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "help.hpp"

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

REGISTER_DIALOG(final_battle)

tfinal_battle::tfinal_battle(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, int side)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams_[side - 1])
	, candidate_cities_()
	, type_index_(0)
{
}

void tfinal_battle::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "city_list", false);

	type_index_ = list.get_selected_row();
}

void tfinal_battle::pre_show(CVideo& /*video*/, twindow& window)
{
	int side_num = current_team_.side();
	std::stringstream str;

	size_t cities = 0;
	for (size_t side = 0; side != teams_.size(); side ++) {
		std::vector<artifical*>& side_cities = teams_[side].holded_cities();
		cities += side_cities.size();
	}
	size_t again = cities / 3 + ((cities % 3)? 1: 0);
	if (again > current_team_.holded_cities().size()) {
		again -= current_team_.holded_cities().size();
	} else {
		again = 0;
	}

	tlabel* label = find_widget<tlabel>(&window, "tip", false, true);
	utils::string_map symbols;
	str << again;
	symbols["count"] = str.str();
	str.str("");
	str << help::tintegrate::generate_format(vgettext("wesnoth-lib", "If capture $count cities again, all AI will ally.", symbols), "red");
	label->set_label(str.str());

	tlistbox* list = find_widget<tlistbox>(&window, "city_list", false, true);

	int index = 0;
	for (unit_map::iterator it = units_.begin(); it != units_.end(); ++it) {
		if (!it->is_city() || it->side() != side_num) {
			continue;
		}
		artifical& city = *unit_2_artifical(&*it);
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = city.absolute_image();
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = city.name();
		list_item_item.insert(std::make_pair("name", list_item));

		// hp
		str.str("");
		str << city.hitpoints() << "/\n" << city.max_hitpoints();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("hp", list_item));

		// xp
		str.str("");
		str << city.experience() << "/\n";
		if (it->can_advance()) {
			str << city.max_experience();
		} else {
			str << "-";
		}
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("xp", list_item));

		str.str("");
		str << city.finish_heros().size() + city.fresh_heros().size();
		list_item["label"] = str.str();
		list_item_item.insert(std::make_pair("reside_hero", list_item));

		list->add_row(list_item_item);

		candidate_cities_.push_back(&city);
	}

	list->set_callback_value_change(dialog_callback<tfinal_battle, &tfinal_battle::type_selected>);

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	if (rpg::stratum != hero_stratum_leader) {
		ok->set_visible(twidget::INVISIBLE);
	}
}

void tfinal_battle::post_show(twindow& window)
{
}

artifical* tfinal_battle::dst_city() const
{ 
	return candidate_cities_[type_index_]; 
}

} // namespace gui2
