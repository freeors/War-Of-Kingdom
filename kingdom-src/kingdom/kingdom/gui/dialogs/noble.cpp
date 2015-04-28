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

#define GETTEXT_DOMAIN "kingdom-lib"

#include "gui/dialogs/noble.hpp"

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
#include "gui/widgets/listbox.hpp"
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

REGISTER_DIALOG(noble2)

tnoble2::tnoble2(std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<std::pair<int, unit*> >& human_pairs, bool replaying)
	: teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams[rpg::h->side_])
	, human_pairs_(human_pairs)
	, replaying_(replaying)
	, holden_()
	, noble_list_index_(-1)
	, human_index_(-1)
	, treasure_list_(NULL)
	, human_list_(NULL)
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

void tnoble2::pre_show(CVideo& /*video*/, twindow& window)
{
	treasure_list_ = find_widget<tlistbox>(&window, "treasure_list", false, true);
	human_list_ = find_widget<tlistbox>(&window, "human_list", false, true);
	
	treasure_list_->set_callback_value_change(dialog_callback3<tnoble2, tlistbox, &tnoble2::treasure_selected>);
	human_list_->set_callback_value_change(dialog_callback3<tnoble2, tlistbox, &tnoble2::human_selected>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "left", false)
		, boost::bind(
			&tnoble2::left
			, this
			, boost::ref(window)));
	find_widget<tbutton>(&window, "left", false).set_active(false);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "right", false)
		, boost::bind(
			&tnoble2::right
			, this
			, boost::ref(window)));
	find_widget<tbutton>(&window, "right", false).set_active(false);

	fill_2list(window);

	noble_list_index_ = treasure_list_->get_selected_row();
	if (human_list_->get_item_count()) {
		human_selected(window, *human_list_);
	}
}

void tnoble2::post_show(twindow& window)
{
}

void tnoble2::fill_2list(twindow& window)
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
			strstr << vgettext2("noble^Lv$level", symbols);
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

			twidget* grid_ptr = treasure_list_->get_row_panel(treasure_list_->get_item_count() - 1);
			ttoggle_panel* toggle = dynamic_cast<ttoggle_panel*>(grid_ptr);
			toggle->set_data(t.index());
		}
	}

	for (std::vector<std::pair<int, unit*> >::iterator itor = human_pairs_.begin(); itor != human_pairs_.end(); ++ itor) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero& h = heros_[itor->first];
		unit& u = *itor->second;

		strstr.str("");
		if (h.number_ == leader.number_) {
			strstr << tintegrate::generate_format(h.name(), "red");
		} else if (h.noble_ != HEROS_NO_NOBLE) {
			strstr << tintegrate::generate_format(h.name(), "green");
		} else {
			strstr << h.name();
		}
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("name", table_item));

		strstr.str("");
		strstr << h.loyalty(leader);
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("loyalty", table_item));

		strstr.str("");
		if (!u.is_artifical()) {
			strstr << u.name() << _("name^Troop");
		} else if (u.is_city()) {
			strstr << u.name();
		} else {
			strstr << u.name() << u.type_name();
		}
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("position", table_item));

		
		strstr.str("");
		strstr << hero::adaptability_str2(h.skill_[hero_skill_formation]);
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("formation", table_item));

		human_list_->add_row(table_item_item);
	}
}

int tnoble2::noble_list_index_2_noble_index() const
{
	twidget* grid_ptr = treasure_list_->get_row_panel(noble_list_index_);
	return dynamic_cast<ttoggle_panel*>(grid_ptr)->get_data();
}

void tnoble2::left(twindow& window)
{
	hero& h = heros_[human_pairs_[human_index_].first];

	int noble_index = noble_list_index_2_noble_index();
	current_team_.appoint_noble(h, noble_index, false);
	holden_.insert(std::make_pair(noble_index, human_pairs_[human_index_]));

	set_internal(window, unit_types.noble(noble_index));
}

void tnoble2::right(twindow& window)
{
	int noble_index = noble_list_index_2_noble_index();
	std::map<int, std::pair<int, unit*> >::iterator it = holden_.find(noble_index);

	hero& h = heros_[it->second.first];
	current_team_.appoint_noble(h, HEROS_NO_NOBLE, false);
	holden_.erase(it);

	set_internal(window, unit_types.noble(noble_index));
}

void tnoble2::set_internal(twindow& window, const tnoble& noble)
{
	treasure_list_->clear();
	human_list_->clear();
	fill_2list(window);

	window.invalidate_layout();
	treasure_list_->select_row(noble_list_index_);

	human_list_->select_row(human_index_);

	const hero& h = heros_[human_pairs_[human_index_].first];
	refresh_3button(window, noble, h);
}

void tnoble2::treasure_selected(twindow& window, tlistbox& list, const int type)
{
	const int cursel = treasure_list_->get_selected_row();

	if (cursel >= 0 && human_list_->get_item_count()) {
		noble_list_index_ = cursel;
		const hero& h = heros_[human_pairs_[human_index_].first];
		refresh_3button(window, unit_types.noble(noble_list_index_2_noble_index()), h);
	}
}

void tnoble2::human_selected(twindow& window, tlistbox& list, const int type)
{
	const int cursel = human_list_->get_selected_row();

	if (cursel >= 0) {
		human_index_ = cursel;
		const hero& h = heros_[human_pairs_[human_index_].first];
		refresh_3button(window, unit_types.noble(noble_list_index_2_noble_index()), h);
	}
}

void tnoble2::refresh_3button(twindow& window, const tnoble& noble, const hero& h)
{
	tbutton* left = find_widget<tbutton>(&window, "left", false, true);
	tbutton* right = find_widget<tbutton>(&window, "right", false, true);
	hero* leader = current_team_.leader();
	
	if (replaying_ || noble.level() > leader_noble_level_) {
		left->set_active(false);
		right->set_active(false);
		return;
	}

	if (h.number_ != leader->number_ && h.noble_ == HEROS_NO_NOBLE && holden_.find(noble.index()) == holden_.end()) {
		left->set_active(true);
	} else {
		left->set_active(false);
	}
	

	std::map<int, std::pair<int, unit*> >::const_iterator it = holden_.find(noble.index());
	if (it != holden_.end() && it->second.second) {
		right->set_active(true);
	} else {
		right->set_active(false);
	}
}

} // namespace gui2
