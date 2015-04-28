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

#include "gui/dialogs/treasure.hpp"

#include "game_display.hpp"
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
#include "gui/dialogs/message.hpp"

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

REGISTER_DIALOG(treasure)

ttreasure::ttreasure(game_display& disp, std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<std::pair<int, unit*> >& human_pairs, bool replaying)
	: disp_(disp)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, current_team_(teams[rpg::h->side_])
	, human_pairs_(human_pairs)
	, replaying_(replaying)
	, treasure_index_(-1)
	, human_index_(-1)
	, treasure_list_(NULL)
	, human_list_(NULL)
{
}

void ttreasure::pre_show(CVideo& /*video*/, twindow& window)
{
	treasure_list_ = find_widget<tlistbox>(&window, "treasure_list", false, true);
	human_list_ = find_widget<tlistbox>(&window, "human_list", false, true);
	
	std::vector<size_t>& holded_treasures = current_team_.holded_treasures();
	if (!holded_treasures.empty()) {
		std::sort(holded_treasures.begin(), holded_treasures.end());
	}

	treasure_list_->set_callback_value_change(dialog_callback3<ttreasure, tlistbox, &ttreasure::treasure_selected>);
	human_list_->set_callback_value_change(dialog_callback3<ttreasure, tlistbox, &ttreasure::human_selected>);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "down", false)
		, boost::bind(
			&ttreasure::down
			, this
			, boost::ref(window)));
	find_widget<tbutton>(&window, "down", false).set_active(false);
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "up", false)
		, boost::bind(
			&ttreasure::up
			, this
			, boost::ref(window)));
	find_widget<tbutton>(&window, "up", false).set_active(false);

	fill_2list(window);

	treasure_index_ = treasure_list_->get_item_count() - 1;
	if (treasure_index_ >= 0) {
		treasure_list_->select_row(treasure_index_);
	}
	if (human_list_->get_item_count()) {
		human_selected(window, *human_list_);
	}
}

void ttreasure::post_show(twindow& window)
{
}

bool can_up_treasure(unit_map& units, const hero& h)
{
	if (tent::tower_mode()) {
		return true;
	}
	unit* u = units.find_unit(h);
	if (u->is_city()) {
		return true;
	}
	artifical* city = units.city_from_loc(u->get_location());
	if (city) {
		return true;
	}
	return false;
}

void ttreasure::fill_2list(twindow& window)
{
	std::stringstream strstr;
	// const std::vector<ttreasure>& treasures = unit_types.treasures();
	const std::vector<size_t>& holded_treasures = current_team_.holded_treasures();
	hero& leader = *current_team_.leader();

	for (std::vector<size_t>::const_iterator it = holded_treasures.begin(); it != holded_treasures.end(); ++ it) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		const ::ttreasure& t = unit_types.treasure(*it);

		strstr.str("");
		strstr << t.image();
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("icon", table_item));

		table_item["label"] = t.name();
		table_item_item.insert(std::make_pair("name", table_item));

		strstr.str("");
		strstr << hero::feature_str(t.feature());
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("feature", table_item));

		treasure_list_->add_row(table_item_item);
	}

	for (std::vector<std::pair<int, unit*> >::iterator itor = human_pairs_.begin(); itor != human_pairs_.end(); ++ itor) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero& h = heros_[itor->first];
		unit& u = *itor->second;

		strstr.str("");
		if (!can_up_treasure(units_, h)) {
			strstr << tintegrate::generate_format(h.name(), "yellow");
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
		if (h.treasure_ != HEROS_NO_TREASURE) {
			const ::ttreasure t = unit_types.treasure(h.treasure_);
			strstr << t.name();
			strstr << "(";
			strstr << hero::feature_str(t.feature());
			strstr << ")";
		}
		table_item["label"] = strstr.str();
		table_item_item.insert(std::make_pair("treasure", table_item));

		human_list_->add_row(table_item_item);
	}
}

void ttreasure::down(twindow& window)
{
	std::vector<size_t>& holded_treasures = current_team_.holded_treasures();
	hero& h = heros_[human_pairs_[human_index_].first];

	if (!can_up_treasure(units_, h)) {
		utils::string_map symbols;
		symbols["treasure"] = tintegrate::generate_format(unit_types.treasure(h.treasure_).name(), "green");
		symbols["hero"] = tintegrate::generate_format(h.name(), "yellow");
		std::string message = vgettext2("Duration this time, if remove $treasure from $hero, cannot assemble treasure to $hero again. Do you want to remove?", symbols);
		int res = gui2::show_message(disp_.video(), "", message, gui2::tmessage::yes_no_buttons);
		if (res == gui2::twindow::CANCEL) {
			return;
		}
	}

	holded_treasures.push_back(h.treasure_);
	h.treasure_ = HEROS_NO_TREASURE;

	std::sort(holded_treasures.begin(), holded_treasures.end());
	set_internal(window);
}

void ttreasure::up(twindow& window)
{
	std::vector<size_t>& holded_treasures = current_team_.holded_treasures();
	hero& h = heros_[human_pairs_[human_index_].first];

	h.treasure_ = holded_treasures[treasure_index_];

	holded_treasures.erase(holded_treasures.begin() + treasure_index_);
	set_internal(window);
}

void ttreasure::set_internal(twindow& window)
{
	const std::vector<size_t>& holded_treasures = current_team_.holded_treasures();

	treasure_list_->clear();
	human_list_->clear();
	fill_2list(window);

	window.invalidate_layout();
	treasure_index_ = treasure_list_->get_item_count() - 1;
	if (treasure_index_ >= 0) {
		treasure_list_->select_row(treasure_index_);
	}
	human_list_->select_row(human_index_);

	const hero& h = heros_[human_pairs_[human_index_].first];
	refresh_3button(window, h);
}

void ttreasure::treasure_selected(twindow& window, tlistbox& list, const int type)
{
	treasure_index_ = treasure_list_->get_selected_row();
}

void ttreasure::human_selected(twindow& window, tlistbox& list, const int type)
{
	human_index_ = human_list_->get_selected_row();

	const hero& h = heros_[human_pairs_[human_index_].first];

	refresh_3button(window, h);
}

void ttreasure::refresh_3button(twindow& window, const hero& h)
{
	std::vector<size_t>& holded_treasures = current_team_.holded_treasures();

	tbutton* down = find_widget<tbutton>(&window, "down", false, true);
	tbutton* up = find_widget<tbutton>(&window, "up", false, true);

	down->set_active(!replaying_ && h.treasure_ != HEROS_NO_TREASURE);
	if (replaying_ || h.treasure_ != HEROS_NO_TREASURE || holded_treasures.empty() || !can_up_treasure(units_, h)) {
		up->set_active(false);
	} else {
		up->set_active(true);
	}
}

} // namespace gui2
