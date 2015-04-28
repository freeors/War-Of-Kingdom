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

#include "gui/dialogs/troop_selection.hpp"

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
#include "gui/widgets/stacked_widget.hpp"
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

REGISTER_DIALOG(troop_selection)

ttroop_selection::ttroop_selection(std::vector<team>* teams, unit_map* units, hero_map& heros, std::vector<std::pair<int, unit*> >& pairs, int side, const std::string& disable_str)
	: teams_(teams)
	, units_(units)
	, heros_(heros)
	, side_(side)
	, pairs_(pairs)
	, max_checked_size_(pairs.size())
	, current_page_(twidget::npos)
{
	disables_ = utils::split(disable_str);
}

void ttroop_selection::pre_show(CVideo& /*video*/, twindow& window)
{
	max_checked_size_ = 1;

	page_panel_ = find_widget<tstacked_widget>(&window, "page", false, true);
	lists_.push_back(find_widget<tlistbox>(&window, "status_list", false, true));
	lists_.push_back(find_widget<tlistbox>(&window, "merit_list", false, true));
	for (std::vector<tlistbox*>::const_iterator it = lists_.begin(); it != lists_.end(); ++ it) {
		tlistbox& list = **it;
		list.set_callback_value_change(dialog_callback3<ttroop_selection, tlistbox, &ttroop_selection::hero_changed>);
	}

	catalog_page(window, STATUS_PAGE, false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "status", false)
		, boost::bind(
			&ttroop_selection::catalog_page
			, this
			, boost::ref(window)
			, (int)STATUS_PAGE
			, true));
	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "merit", false)
		, boost::bind(
			&ttroop_selection::catalog_page
			, this
			, boost::ref(window)
			, (int)MERIT_PAGE
			, true));

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(false);
}

void ttroop_selection::post_show(twindow& window)
{
}

void ttroop_selection::hero_changed(twindow& window, tlistbox& list, const int type)
{
	int selected_row = list.get_selected_row();

	twindow::tinvalidate_layout_blocker invalidate_layout_blocker(window);

	tcontrol* portrait = find_widget<tcontrol>(&window, "portrait", false, true);

	const unit* troop = pairs_[selected_row].second;

	hero& h = troop->master();
	portrait->set_label(h.image(true));

	portrait = find_widget<tcontrol>(&window, "second_portrait", false, true);
	if (troop->second().valid()) {
		portrait->set_label(troop->second().image());
	} else {
		portrait->set_label("");
	}

	portrait = find_widget<tcontrol>(&window, "third_portrait", false, true);
	if (troop->third().valid()) {
		portrait->set_label(troop->third().image());
	} else {
		portrait->set_label("");
	}
}

void ttroop_selection::hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	std::set<size_t>::iterator result = checked_pairs_.find(toggled_index);

	if (result == checked_pairs_.end()) {
		// toggled button isn't in checked_heros_
		if (toggle->get_value()) {
			if (checked_pairs_.size() < max_checked_size_) {
				checked_pairs_.insert(toggled_index);
			} else {
				// At most select max_checked_size_ heros. decheck it!
				toggle->set_value(false);
				return;
			}			
		} else {
			VALIDATE(false, "hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "hero_toggled program error #2");
	} else {
		checked_pairs_.erase(result);
	}

	twindow* window = toggle->get_window();
	tbutton* ok = find_widget<tbutton>(window, "ok", false, true);
	ok->set_active(checked_pairs_.empty()? false: true);
}

void ttroop_selection::fill_table(tlistbox& list, int catalog)
{
	std::vector<int> features;
	std::stringstream str;

	hero& leader = *(*teams_)[side_ - 1].leader();
	int hero_index = 0;
	for (std::vector<std::pair<int, unit*> >::iterator itor = pairs_.begin(); itor != pairs_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		unit& u = *itor->second;

		str.str("");
		str << u.name();
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("name", table_item));

		if (catalog == STATUS_PAGE) {
			str.str("");
			str << u.type_name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("type", table_item));

			str.str("");
			str << u.level();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("level", table_item));

			str.str("");
			str << u.hitpoints()  << "/" << u.max_hitpoints();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hp", table_item));

			str.str("");
			str << u.experience() << "/";
			if (u.can_advance()) {
				str << u.max_experience();
			} else {
				str << "-";
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("xp", table_item));

			str.str("");
			str << utils::join(u.trait_names(), ", ");
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("traits", table_item));

			str.str("");
			str << u.movement_left() << "/" << u.total_movement();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("movement", table_item));

		} else if (catalog == MERIT_PAGE) {
			str.str("");
			str << u.type_name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("type", table_item));

			str.str("");
			str << u.cause_damage_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("cause_damage", table_item));

			str.str("");
			str << u.been_damage_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("been_damage", table_item));

			str.str("");
			str << u.defeat_units_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("defeat_units", table_item));

			str.str("");
			str << u.field_turns_;
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("field_turns", table_item));
		}
		list.add_row(table_item_item);

		twidget* grid_ptr = list.get_row_panel(hero_index);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&ttroop_selection::hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (checked_pairs_.find(hero_index) != checked_pairs_.end()) {
			toggle->set_value(true);
		}
	}
}

void ttroop_selection::catalog_page(twindow& window, int page, bool swap)
{
	if (!page_panel_) {
		return;
	}
	if (current_page_ == page) {
		return;
	}

	int selected_row = 0;
	if (swap) {
		// because sort, order is changed.
		tlistbox& list = *lists_[current_page_];
		selected_row = list.get_selected_row();
		list.clear();
	}

	page_panel_->set_radio_layer(page);
	tlistbox& list = *lists_[page];

	current_page_ = page;

	fill_table(list, page);

	if (swap) {
		list.select_row(selected_row);
		list.invalidate_layout(true);
	} else {
		hero_changed(window, list);
	}
}

} // namespace gui2
