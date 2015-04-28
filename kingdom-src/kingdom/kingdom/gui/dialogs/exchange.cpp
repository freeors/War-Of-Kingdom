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

#include "gui/dialogs/exchange.hpp"

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

REGISTER_DIALOG(exchange)

texchange::texchange(std::vector<team>& teams, unit_map& units, hero_map& heros, std::vector<std::pair<size_t, unit*> >& human_pairs, std::vector<std::pair<size_t, unit*> >& ai_pairs)
	: teams_(teams)
	, units_(units)
	, heros_(heros)
	, human_pairs_(human_pairs)
	, ai_pairs_(ai_pairs)
	, checked_human_pairs_()
	, checked_ai_pairs_()
	, max_checked_human_size_(0)
	, max_checked_ai_size_(0)
	, human_list_(NULL)
	, ai_list_(NULL)
{
}

void texchange::pre_show(CVideo& /*video*/, twindow& window)
{
	max_checked_human_size_ = 3;
	max_checked_ai_size_ = 1;

	human_list_ = find_widget<tlistbox>(&window, "human_list", false, true);
	ai_list_ = find_widget<tlistbox>(&window, "ai_list", false, true);

	std::stringstream str;

	hero& leader = *teams_[rpg::h->side_].leader();
	int hero_index = 0;
	for (std::vector<std::pair<size_t, unit*> >::iterator itor = human_pairs_.begin(); itor != human_pairs_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero& h = heros_[itor->first];
		unit& u = *itor->second;

		int catalog_diff = posix_abs((int)leader.base_catalog_ - h.base_catalog_);
		if (catalog_diff > HERO_MAX_LOYALTY / 2) {
			catalog_diff = HERO_MAX_LOYALTY - catalog_diff;
		}

		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		artifical* city = units_.city_from_cityno(h.city_);
		if (city) {
			table_item["label"] = city->name();
		} else {
			table_item["label"] = "---";
		}
		table_item_item.insert(std::make_pair("city", table_item));

		str.str("");
		str << h.loyalty(leader);
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("loyalty", table_item));

		str.str("");
		str << (int)h.base_catalog_;
		str << "(" << catalog_diff << ")";
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("hero_catalog", table_item));

		str.str("");
		if (!u.is_artifical()) {
			str << u.name() << _("name^Troop");
		} else if (u.is_city()) {
			str << u.name();
		} else {
			str << u.name() << u.type_name();
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("position", table_item));

		human_list_->add_row(table_item_item);

		twidget* grid_ptr = human_list_->get_row_panel(human_list_->get_item_count() - 1);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&texchange::human_hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (h.number_ == leader.number_ || h.number_ == rpg::h->number_) {
			toggle->set_active(false);
		}
	}

	hero_index = 0;
	for (std::vector<std::pair<size_t, unit*> >::iterator itor = ai_pairs_.begin(); itor != ai_pairs_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero& h = heros_[itor->first];
		unit& u = *itor->second;

		int catalog_diff = posix_abs((int)leader.base_catalog_ - h.base_catalog_);
		if (catalog_diff > HERO_MAX_LOYALTY / 2) {
			catalog_diff = HERO_MAX_LOYALTY - catalog_diff;
		}

		table_item["label"] = h.name();
		table_item_item.insert(std::make_pair("name", table_item));

		artifical* city = units_.city_from_cityno(h.city_);
		if (city) {
			table_item["label"] = city->name();
		} else {
			table_item["label"] = "---";
		}
		table_item_item.insert(std::make_pair("city", table_item));

		str.str("");
		str << h.loyalty(leader);
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("loyalty", table_item));

		str.str("");
		str << (int)h.base_catalog_;
		str << "(" << catalog_diff << ")";
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("hero_catalog", table_item));

		str.str("");
		if (!u.is_artifical()) {
			str << u.name() << _("name^Troop");
		} else if (u.is_city()) {
			str << u.name();
		} else {
			str << u.name() << u.type_name();
		}
		table_item["label"] = str.str();
		table_item_item.insert(std::make_pair("position", table_item));

		ai_list_->add_row(table_item_item);

		twidget* grid_ptr = ai_list_->get_row_panel(ai_list_->get_item_count() - 1);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&texchange::ai_hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (h.number_ == leader.number_ || h.number_ == rpg::h->number_) {
			toggle->set_active(false);
		}
	}

	tbutton* ok = find_widget<tbutton>(&window, "ok", false, true);
	ok->set_active(false);
}

void texchange::post_show(twindow& window)
{
}

void texchange::human_hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	std::set<size_t>::iterator result = checked_human_pairs_.find(toggled_index);

	if (result == checked_human_pairs_.end()) {
		// toggled button isn't in checked_heros_
		if (toggle->get_value()) {
			if (checked_human_pairs_.size() < max_checked_human_size_) {
				checked_human_pairs_.insert(toggled_index);
			} else {
				// At most select max_checked_human_size_ heros. decheck it!
				toggle->set_value(false);
				return;
			}			
		} else {
			VALIDATE(false, "human_hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "human_hero_toggled program error #2");
	} else {
		checked_human_pairs_.erase(result);
	}

	twindow* window = toggle->get_window();
	tbutton* ok = find_widget<tbutton>(window, "ok", false, true);
	ok->set_active(checked_human_pairs_.size() == max_checked_human_size_ && checked_ai_pairs_.size() == max_checked_ai_size_);
}

void texchange::ai_hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();
	std::set<size_t>::iterator result = checked_ai_pairs_.find(toggled_index);

	if (result == checked_ai_pairs_.end()) {
		// toggled button isn't in checked_heros_
		if (toggle->get_value()) {
			if (checked_ai_pairs_.size() < max_checked_ai_size_) {
				checked_ai_pairs_.insert(toggled_index);
			} else {
				// At most select max_checked_ai_size_ heros. decheck it!
				toggle->set_value(false);
				return;
			}			
		} else {
			VALIDATE(false, "ai_hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "ai_hero_toggled program error #2");
	} else {
		checked_ai_pairs_.erase(result);
	}

	twindow* window = toggle->get_window();
	tbutton* ok = find_widget<tbutton>(window, "ok", false, true);
	ok->set_active(checked_human_pairs_.size() == max_checked_human_size_ && checked_ai_pairs_.size() == max_checked_ai_size_);
}

} // namespace gui2
