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

#include "gui/dialogs/interior.hpp"

#include "actions.hpp"
#include "foreach.hpp"
#include "formula_string_utils.hpp"
#include "gettext.hpp"
#include "game_display.hpp"
#include "team.hpp"
#include "hero.hpp"
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
#include "gui/widgets/stacked_widget.hpp"
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

REGISTER_DIALOG(interior)

tinterior::tinterior(game_display& gui, std::vector<team>& teams, unit_map& units, hero_map& heros, artifical& city, bool browse)
	: gui_(gui)
	, teams_(teams)
	, units_(units)
	, heros_(heros)
	, city_(city)
	, current_team_(teams_[city.side() - 1])
	, browse_(browse)
	, fresh_heros_()
	, type_index_(0)
	, checked_hero_(-1)
	, appoint_(NULL)
	, hero_table_(NULL)
	, market_map_()
	, technology_map_()
{
	departments_.push_back(department(department::commercial, _("Commercial"), "interior/commercial.png", "interior/commercial-unit.png"));
	hero& h1 = city_.master();
	hero& h2 = *city_.mayor();
	departments_.back().exploiture_ = calculate_exploiture(h1, h2, department::commercial);

	departments_.push_back(department(department::technology, _("Technology"), "interior/technology.png", "interior/technology-unit.png"));
	departments_.back().exploiture_ = calculate_exploiture(h1, h2, department::technology);
}

void tinterior::type_selected(twindow& window)
{
	tlistbox& list = find_widget<tlistbox>(&window, "type_list", false);
	type_index_ = list.get_selected_row();
	
	// appoint_->set_active(!officials.empty());
}

void tinterior::hero_toggled(twidget* widget)
{
	ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(widget);
	int toggled_index = toggle->get_data();

	if (toggled_index != checked_hero_) {
		// toggled button isn't in checked_hero_
		if (toggle->get_value()) {
			if (checked_hero_ == -1) {
				checked_hero_ = toggled_index;
			} else {
				// At most select three heros. decheck it!
				toggle->set_value(false);
				return;
			}
		} else {
			VALIDATE(false, "hero_toggled program error #1");
		}
	} else if (toggle->get_value()) { 
		VALIDATE(false, "hero_toggled program error #2");
	} else {
		checked_hero_ = -1;
	}

	twindow* window = toggle->get_window();

	bool active = true;
	hero* mayor = city_.mayor();
	if ((!mayor->valid() && checked_hero_ == -1) || (mayor->valid() && checked_hero_ != -1 && mayor->number_ == fresh_heros_[checked_hero_]->number_)) {
		active = false;
	}

	appoint_->set_active(active);

	refresh_tooltip(*window);
}

void tinterior::refresh_tooltip(twindow& window)
{
	std::stringstream strstr;

	// refresh to gui
	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);
	hero* mayor = city_.mayor();

	for (std::vector<department>::const_iterator d_it = departments_.begin(); d_it != departments_.end(); ++ d_it) {
		tgrid* grid_ptr = list->get_row_grid(d_it->type_);

		tcontrol* control = find_widget<tcontrol>(grid_ptr, "new_exploiture", false, true);
		control->set_use_markup(true);
		strstr.str("");
		if ((!mayor->valid() && checked_hero_ == -1) || (mayor->valid() && checked_hero_ != -1 && mayor->number_ == fresh_heros_[checked_hero_]->number_)) {
			strstr << d_it->exploiture_ << "%";
		} else {
			int exploiture = calculate_exploiture(city_.master(), checked_hero_ != -1? *(fresh_heros_[checked_hero_]): hero_invalid, d_it->type_);
			if (exploiture > d_it->exploiture_) {
				strstr << "<0,255,0>";
			} else if (exploiture < d_it->exploiture_) {
				strstr << "<255,0,0>";
			}
			strstr << exploiture << "%";
		}
		control->set_label(strstr.str());
	}
}

void tinterior::appoint(twindow& window)
{
	std::stringstream strstr;
	hero* new_mayor = checked_hero_ != -1? fresh_heros_[checked_hero_]: &hero_invalid;
	city_.select_mayor(new_mayor, false);

	tcontrol* control;
	for (int index = 0; index < (int)fresh_heros_.size(); index ++) {
		tgrid* grid_ptr = hero_table_->get_row_grid(index);
		control = dynamic_cast<tcontrol*>(grid_ptr->find("name", true));
		hero& h = *fresh_heros_[index];
		strstr.str("");
		if (h.official_ == hero_official_mayor) {
			strstr << "<0,0,255>";
		}
		strstr << h.name();
		control->set_label(strstr.str());
	}

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	// official portrait
	control = find_widget<tcontrol>(&window, "mayor", false, true);
	if (new_mayor->valid()) {
		control->set_label(new_mayor->image());
	} else {
		control->set_label("");
	}

	for (std::vector<department>::iterator d_it = departments_.begin(); d_it != departments_.end(); ++ d_it) {
		tgrid* grid_ptr = list->get_row_grid(d_it->type_);

		d_it->exploiture_ = calculate_exploiture(city_.master(), new_mayor? (*new_mayor): hero_invalid, d_it->type_);

		// exploiture data
		strstr.str("");
		if (d_it->type_ == department::commercial) {
			strstr << calculate_markets(d_it->exploiture_).second;
		} else {
			strstr << calculate_technologies(d_it->exploiture_).second;
		}
		control = dynamic_cast<tcontrol*>(grid_ptr->find("total", true));
		control->set_label(strstr.str());

		strstr.str("");
		strstr << d_it->exploiture_ << "%";
		control = dynamic_cast<tcontrol*>(grid_ptr->find("exploiture", true));
		control->set_label(strstr.str());
	}

	appoint_->set_active(false);
	refresh_tooltip(window);
}

std::pair<int, int> tinterior::calculate_markets(int exploiture)
{
	int count = 0, income = 0;
	for (std::map<const unit_type*, int>::const_iterator it2 = market_map_.begin(); it2 != market_map_.end(); ++ it2) {
		for (int i = 0; i < it2->second; i ++) {
			income += it2->first->gold_income() * exploiture / 100;
		}
		count += it2->second;
	}
	return std::make_pair(count, income);
}

std::pair<int, int> tinterior::calculate_technologies(int exploiture)
{
	int count = 0, income = 0;
	for (std::map<const unit_type*, int>::const_iterator it2 = technology_map_.begin(); it2 != technology_map_.end(); ++ it2) {
		for (int i = 0; i < it2->second; i ++) {
			income += it2->first->technology_income() * exploiture / 100;
		}
		count += it2->second;
	}
	return std::make_pair(count, income);
}

void tinterior::pre_show(CVideo& /*video*/, twindow& window)
{
	int side_num = current_team_.side();
	std::stringstream strstr;

	tlabel* label = find_widget<tlabel>(&window, "title", false, true);
	strstr.str();
	strstr << _("Interior") << "(" << city_.name() << ")";
	label->set_label(strstr.str());
	if (browse_) {
		label = find_widget<tlabel>(&window, "flag", false, true);
		strstr.str("");
		strstr << "(" << _("Browse") << ")";
		label->set_label(strstr.str());
	}

	const unit_type_data::unit_type_map& types = unit_types.types();
	for (unit_type_data::unit_type_map::const_iterator it = types.begin(); it != types.end(); ++ it) {
		if (it->second.master() == hero::number_market) {
			if (market_map_.find(&it->second) != market_map_.end()) {
				continue;
			}
			market_map_.insert(std::make_pair(&it->second, 0));
		}
		if (it->second.master() == hero::number_technology) {
			if (technology_map_.find(&it->second) != technology_map_.end()) {
				continue;
			}
			technology_map_.insert(std::make_pair(&it->second, 0));
		}
	}

	// candidate heros
	const std::vector<hero*>& freshes = city_.fresh_heros();
	std::copy(freshes.begin(), freshes.end(), std::back_inserter(fresh_heros_));
	const std::vector<hero*>& finishes = city_.finish_heros();
	std::copy(finishes.begin(), finishes.end(), std::back_inserter(fresh_heros_));
	for (std::vector<unit*>::iterator it = city_.reside_troops().begin(); it != city_.reside_troops().end(); ++ it) {
		unit& u = **it;
		hero* h = &u.master();
		fresh_heros_.push_back(h);
		if (u.second().valid()) {
			h = &u.second();
			fresh_heros_.push_back(h);
		}
		if (u.third().valid()) {
			h = &u.third();
			fresh_heros_.push_back(h);
		}
	}
	for (std::vector<unit*>::iterator it = city_.field_troops().begin(); it != city_.field_troops().end(); ++ it) {
		unit& u = **it;
		hero* h = &u.master();
		fresh_heros_.push_back(h);
		if (u.second().valid()) {
			h = &u.second();
			fresh_heros_.push_back(h);
		}
		if (u.third().valid()) {
			h = &u.third();
			fresh_heros_.push_back(h);
		}
	}

	// calculate artificals
	const std::vector<map_location>& economy_area = city_.economy_area();
	for (std::vector<map_location>::const_iterator it = economy_area.begin(); it != economy_area.end(); ++ it) {
		unit_map::const_iterator find = units_.find(*it);
		if (!find.valid()) {
			continue;
		}
		std::map<const unit_type*, int>::iterator find2 = market_map_.find(find->type());
		if (find2 != market_map_.end()) {
			find2->second ++;
			continue;
		}
		find2 = technology_map_.find(find->type());
		if (find2 != technology_map_.end()) {
			find2->second ++;
		}
	}

	std::sort(fresh_heros_.begin(), fresh_heros_.end(), compare_politics);

	// mayor
	tcontrol* control = find_widget<tcontrol>(&window, "mayor", false, true);
	hero* mayor = city_.mayor();
	if (mayor->valid()) {
		control->set_label(mayor->image());
	} else {
		control->set_label("");
	}

	tlistbox* list = find_widget<tlistbox>(&window, "type_list", false, true);

	for (std::vector<department>::const_iterator d_it = departments_.begin(); d_it != departments_.end(); ++ d_it) {
		/*** Add list item ***/
		string_map list_item;
		std::map<std::string, string_map> list_item_item;

		list_item["label"] = d_it->image_;
		list_item_item.insert(std::make_pair("icon", list_item));

		list_item["label"] = d_it->name_;
		list_item_item.insert(std::make_pair("name", list_item));

		list_item["label"] = d_it->portrait_;
		list_item_item.insert(std::make_pair("portrait", list_item));

		strstr.str("");
		strstr << "X";
		if (d_it->type_ == department::commercial) {
			strstr << calculate_markets(d_it->exploiture_).first;
		} else if (d_it->type_ == department::technology) {
			strstr << calculate_technologies(d_it->exploiture_).first;
		}
		strstr << "=";
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("number", list_item));

		strstr.str("");
		if (d_it->type_ == department::commercial) {
			strstr << calculate_markets(d_it->exploiture_).second;
		} else if (d_it->type_ == department::technology) {
			strstr << calculate_technologies(d_it->exploiture_).second;
		}
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("total", list_item));

		strstr.str("");
		strstr << d_it->exploiture_ << "%";
		list_item["label"] = strstr.str();
		list_item_item.insert(std::make_pair("exploiture", list_item));

		list->add_row(list_item_item);
	}

	appoint_ = find_widget<tbutton>(&window, "appoint", false, true);
	appoint_->set_active(false);

	// default: 0th department
	if (mayor->valid()) {
		for (std::vector<hero*>::iterator it = fresh_heros_.begin(); it != fresh_heros_.end(); ++ it) {
			if (*it == mayor) {
				checked_hero_ = std::distance(fresh_heros_.begin(), it);
				break;
			}
		}
		VALIDATE(checked_hero_ != -1, "exist mayor(" + mayor->name() + "), but cannot find mayor in candidate hero.");
	}

	// type_selected(window);
	refresh_tooltip(window);

	list->set_callback_value_change(dialog_callback<tinterior, &tinterior::type_selected>);

	hero_table_ = find_widget<tlistbox>(&window, "hero_table", false, true);
	
	// fill data to hero_table
	catalog_page(window, ABILITY_PAGE, false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "ability", false)
		, boost::bind(
			&tinterior::catalog_page
			, this
			, boost::ref(window)
			, (int)ABILITY_PAGE
			, true));
	if (fresh_heros_.empty()) {
		find_widget<tbutton>(&window, "ability", false).set_active(false);
	}

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "adaptability", false)
		, boost::bind(
			&tinterior::catalog_page
			, this
			, boost::ref(window)
			, (int)ADAPTABILITY_PAGE
			, true));
	if (fresh_heros_.empty()) {
		find_widget<tbutton>(&window, "adaptability", false).set_active(false);
	}

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "relation", false)
		, boost::bind(
			&tinterior::catalog_page
			, this
			, boost::ref(window)
			, (int)RELATION_PAGE
			, true));
	if (fresh_heros_.empty()) {
		find_widget<tbutton>(&window, "relation", false).set_active(false);
	}

	connect_signal_mouse_left_click(
		*appoint_
		, boost::bind(
			&tinterior::appoint
			, this
			, boost::ref(window)));
}

void tinterior::post_show(twindow& window)
{
}

void tinterior::catalog_page(twindow& window, int catalog, bool swap)
{
	if (catalog < MIN_PAGE || catalog > MAX_PAGE) {
		return;
	}
	int index = catalog - MIN_PAGE;
	
	if (window.alternate_index() == index) {
		// desired page is the displaying page, do nothing.
		return;
	}

	std::vector<int> features;
	std::stringstream str;

	int selected_row = 0;
	if (swap) {
		selected_row = hero_table_->get_selected_row();
	}

	window.alternate_uh(hero_table_, index);

	int hero_index = 0;
	for (std::vector<hero*>::iterator itor = fresh_heros_.begin(); itor != fresh_heros_.end(); ++ itor, hero_index ++) {
		/*** Add list item ***/
		string_map table_item;
		std::map<std::string, string_map> table_item_item;

		hero* h = *itor;

		if (catalog == ABILITY_PAGE) {
			str.str("");
			table_item["use_markup"] = "true";
			if (h->official_ == hero_official_mayor) {
				str << "<0,0,255>";
			} else if (h->official_ != HEROS_NO_OFFICIAL) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));
			
			table_item["label"] = lexical_cast<std::string>(h->loyalty(*teams_[h->side_].leader()));;
			table_item_item.insert(std::make_pair("loyalty", table_item));

			str.str("");
			str << fxptoi9(h->politics_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("politics", table_item));

			str.str("");
			str << hero::adaptability_str2(h->skill_[hero_skill_commercial]);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("commercial", table_item));

			str.str("");
			str << hero::adaptability_str2(h->skill_[hero_skill_invent]);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("technology", table_item));

			table_item["label"] = hero::feature_str(h->feature_);
			table_item_item.insert(std::make_pair("feature", table_item));

			str.str("");
			str << fxptoi9(h->leadership_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("leadership", table_item));

			str.str("");
			str << fxptoi9(h->intellect_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intellect", table_item));

			str.str("");
			str << fxptoi9(h->charm_);
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("charm", table_item));

		} else if (catalog == ADAPTABILITY_PAGE) {
			str.str("");
			table_item["use_markup"] = "true";
			if (h->official_ == hero_official_mayor) {
				str << "<0,0,255>";
			} else if (h->official_ != HEROS_NO_OFFICIAL) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[0]);
			table_item_item.insert(std::make_pair("arm0", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[1]);
			table_item_item.insert(std::make_pair("arm1", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[2]);
			table_item_item.insert(std::make_pair("arm2", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[3]);
			table_item_item.insert(std::make_pair("arm3", table_item));

			table_item["label"] = hero::adaptability_str2(h->arms_[4]);
			table_item_item.insert(std::make_pair("arm4", table_item));

		} else if (catalog == RELATION_PAGE) {
			str.str("");
			table_item["use_markup"] = "true";
			if (h->official_ == hero_official_mayor) {
				str << "<0,0,255>";
			} else if (h->official_ != HEROS_NO_OFFICIAL) {
				str << "<255,0,0>";
			}
			str << h->name();
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("name", table_item));

			if (h->parent_[0].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[0].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("father", table_item));

			if (h->parent_[1].hero_ != HEROS_INVALID_NUMBER) {
				table_item["label"] = heros_[h->parent_[1].hero_].name();
			} else {
				table_item["label"] = "";
			}
			table_item_item.insert(std::make_pair("mother", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_OATH; i ++) {
				if (h->oath_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->oath_[i].hero_].name();
					} else {
						str << " " << heros_[h->oath_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("oath", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_CONSORT; i ++) {
				if (h->consort_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->consort_[i].hero_].name();
					} else {
						str << " " << heros_[h->consort_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("consort", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_INTIMATE; i ++) {
				if (h->intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->intimate_[i].hero_].name();
					} else {
						str << " " << heros_[h->intimate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("intimate", table_item));

			str.str("");
			for (uint32_t i = 0; i < HEROS_MAX_HATE; i ++) {
				if (h->hate_[i].hero_ != HEROS_INVALID_NUMBER) {
					if (i == 0) {
						str << heros_[h->hate_[i].hero_].name();
					} else {
						str << " " << heros_[h->hate_[i].hero_].name();
					}
				}
			}
			table_item["label"] = str.str();
			table_item_item.insert(std::make_pair("hate", table_item));

		}
		hero_table_->add_row(table_item_item);

		tgrid* grid_ptr = hero_table_->get_row_grid(hero_index);
		ttoggle_button* toggle = dynamic_cast<ttoggle_button*>(grid_ptr->find("prefix", true));
		toggle->set_callback_state_change(boost::bind(&tinterior::hero_toggled, this, _1));
		toggle->set_data(hero_index);
		if (checked_hero_ == hero_index) {
			toggle->set_value(true);
		}
		if (browse_) {
			toggle->set_active(false);
		} else if (h->official_ != HEROS_NO_OFFICIAL && h->official_ != hero_official_mayor) {
			toggle->set_active(false);
		} else if (h->loyalty(*current_team_.leader()) < game_config::wander_loyalty_threshold) {
			toggle->set_active(false);
		}
	}
	if (swap) {
		window.alternate_bh(hero_table_, index);
		hero_table_->select_row(selected_row);
	}
}

} // namespace gui2
