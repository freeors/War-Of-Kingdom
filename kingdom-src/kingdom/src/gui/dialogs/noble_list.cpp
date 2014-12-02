/* $Id: hero_list.cpp 49598 2011-05-22 17:55:54Z mordante $ */
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

#include "gui/dialogs/noble_list.hpp"

#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "team.hpp"
#include "artifical.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"
#include "gui/widgets/scroll_label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

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

REGISTER_DIALOG(noble_list)

tnoble_list::tnoble_list(std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<std::pair<int, unit*> >& human_pairs, int side)
	: teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams[side - 1])
	, human_pairs_(human_pairs)
	, holden_()
	, noble_list_index_(-1)
	, treasure_list_(NULL)
{
	hero& leader = *current_team_.leader();
	leader_noble_level_ = unit_types.noble(leader.noble_).level();

	for (std::vector<std::pair<int, unit*> >::iterator it = human_pairs_.begin(); it != human_pairs_.end(); ++ it) {
		hero& h = heros_[it->first];

		if (h.number_ == leader.number_) {
			continue;
		}
		
		if (h.noble_ != HEROS_NO_NOBLE) {
			holden_.insert(std::make_pair(h.noble_, *it));
		}
	}

	const std::map<int, hero*>& appointed_nobles = current_team_.appointed_nobles();
	for (std::map<int, hero*>::const_iterator it = appointed_nobles.begin(); it != appointed_nobles.end(); ++ it) {
		if (holden_.find(it->first) == holden_.end()) {
			holden_.insert(std::make_pair(it->first, std::make_pair(it->second->number_, reinterpret_cast<unit*>(NULL))));
		}
	}
}

void tnoble_list::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel* title = find_widget<tlabel>(&window, "title", false, true);
	std::stringstream strstr;
	strstr << dsgettext("wesnoth-hero", "noble") << "(" << current_team_.name() << ")";
	title->set_label(strstr.str());
	

	treasure_list_ = find_widget<tlistbox>(&window, "treasure_list", false, true);
	
	treasure_list_->set_callback_value_change(dialog_callback<tnoble_list, &tnoble_list::treasure_selected>);

	fill_2list(window);

	noble_list_index_ = treasure_list_->get_selected_row();
}

void tnoble_list::post_show(twindow& window)
{
}

void tnoble_list::fill_2list(twindow& window)
{
	std::stringstream strstr;
	utils::string_map symbols;
	hero& leader = *current_team_.leader();

	for (int level = game_config::max_noble_level; level >= 0; level --) {
		const std::set<tnoble>& level_nobles = unit_types.level_nobles(level);
		for (std::set<tnoble>::const_iterator it = level_nobles.begin(); it != level_nobles.end(); ++ it) {
			/*** Add list item ***/
			string_map table_item;
			std::map<std::string, string_map> table_item_item;

			const tnoble& t = *it;
			if (t.leader()) {
				continue;
			}
			
			strstr.str("");
			if (t.level() > leader_noble_level_) {
				strstr << tintegrate::generate_format(t.name(), "red");
			} else {
				strstr << t.name();
			}
			table_item["label"] = strstr.str();
			table_item_item.insert(std::make_pair("name", table_item));

			strstr.str("");
			symbols["level"] = lexical_cast_default<std::string>(unit_types.max_noble_level() - t.level() + 1);
			strstr << vgettext("wesnoth-lib", "noble^Lv$level", symbols);
			table_item["label"] = strstr.str();
			table_item_item.insert(std::make_pair("level", table_item));

			strstr.str("");
			strstr << (t.formation()? _("Yes"): _("No"));
			table_item["label"] = strstr.str();
			table_item_item.insert(std::make_pair("formation", table_item));

			strstr.str("");
			std::map<int, std::pair<int, unit*> >::const_iterator find = holden_.find(t.index());
			if (find != holden_.end()) {
				if (!find->second.second) {
					strstr << tintegrate::generate_format(heros_[find->second.first].name(), "red");
				} else {
					strstr << heros_[find->second.first].name();
				}
			}
			table_item["label"] = strstr.str();
			table_item_item.insert(std::make_pair("hero", table_item));

			treasure_list_->add_row(table_item_item);

			tgrid* grid_ptr = treasure_list_->get_row_grid(treasure_list_->get_item_count() - 1);
			ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true));
			toggle->set_data(t.index());
		}
	}
}

int tnoble_list::noble_list_index_2_noble_index() const
{
	tgrid* grid_ptr = treasure_list_->get_row_grid(noble_list_index_);
	return dynamic_cast<ttoggle_panel*>(grid_ptr->find("_toggle", true))->get_data();
}

void tnoble_list::treasure_selected(twindow& window)
{
	noble_list_index_ = treasure_list_->get_selected_row();
}

} // namespace gui2
